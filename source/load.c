#include <miniz/miniz.h>
#include <psp2/kernel/error.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "arm-encode.h"
#include "load.h"
#include "load_exports.h"
#include "load_layout.h"
#include "load_format.h"
#include "load_retry.h"
#include "guest_tp_plan.h"
#include "load_segment.h"
#include "load_module_info.h"
#include "log.h"
#include "module.h"
#include "sce-elf.h"
#include "self.h"
#include "util.h"

static int load_vpk(LoadImage *image, const void *data, uint32_t size, void **entry);
static int load_executable(LoadImage *image, const uint8_t *data, size_t size, void **entry);
static int load_elf(LoadImage *image, const void *data, size_t size, void **entry);
static int load_self(LoadImage *image, const void *data, size_t size, void **entry);
static int load_segments(LoadImage *image, void **entry, Elf32_Addr e_entry, segment_info_t *segments,
                         int num_segments, Elf32_Half type);


typedef struct {
    const segment_info_t *segments;
    int count;
} ExportReader;

static const void *read_export_address(void *context, uint32_t address, size_t size,
                                       bool executable)
{
    const ExportReader *reader = context;
    for (int i = 0; i < reader->count; ++i) {
        const segment_info_t *segment = &reader->segments[i];
        if (segment->p_type != PT_LOAD || (executable && !(segment->p_flags & PF_X)))
            continue;
        LoadAddressRange range = {segment->rx_addr, segment->rw_addr, segment->p_memsz};
        const void *alias = load_address_translate(&range, 1, address, size);
        if (alias) return alias;
    }
    return NULL;
}

int elf_check_vita_header(const Elf32_Ehdr *hdr)
{
    if (!(hdr->e_ident[EI_MAG0] == ELFMAG0 && hdr->e_ident[EI_MAG1] == ELFMAG1 &&
          hdr->e_ident[EI_MAG2] == ELFMAG2 && hdr->e_ident[EI_MAG3] == ELFMAG3)) {
        LOG("Invalid ELF magic number.");
        return -1;
    }

    if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        LOG("Not a 32bit executable.");
        return -1;
    }

    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        LOG("Not a valid ARM executable.");
        return -1;
    }

    if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        LOG("Unsupported ELF version.");
        return -1;
    }

    if (hdr->e_type != ET_SCE_RELEXEC && hdr->e_type != ET_SCE_EXEC) {
        LOG("Only ET_SCE_EXEC and ET_SCE_RELEXEC files are supported.");
        return -1;
    }

    if (hdr->e_machine != EM_ARM) {
        LOG("Not an ARM executable.");
        return -1;
    }

    if (hdr->e_version != EV_CURRENT) {
        LOG("Unsupported ELF version.");
        return -1;
    }

    return 0;
}

static int elf_get_sce_module_info(Elf32_Addr e_entry, const segment_info_t *segments, int num_segments,
                                   sce_module_info_raw **mod_info)
{
    uint32_t offset;
    uint32_t index;

    index = (e_entry & 0xC0000000) >> 30;
    offset = e_entry & 0x3FFFFFFF;

    if (index >= (uint32_t)num_segments || segments[index].p_type != PT_LOAD ||
        segments[index].src_data == NULL || offset > segments[index].p_filesz ||
        sizeof(**mod_info) > segments[index].p_filesz - offset) {
        LOG("Invalid segment index %" PRId32, index);
        return -1;
    }

    *mod_info = (sce_module_info_raw *)((char *)segments[index].src_data + offset);

    return index;
}

int load_file(LoadImage *image, const char *filename, void **entry)
{
    if (!entry) return -1;
    *entry = NULL;
    int result = load_file_prepare(image, filename);
    if (result < 0) return result;
    if (!image->entry || load_image_bind(image, &image->exports) < 0) {
        Result close = load_image_close(image);
        if (R_FAILED(close)) LOG("Executable binding rollback failed: 0x%x", close);
        return -1;
    }
    *entry = image->entry;
    return 0;
}

static int load_file_prepare_kind(LoadImage *image, const char *filename, bool module)
{
    void *data;
    uint32_t size;
    int ret = 0;
    void *entry = NULL;
    if (!image || !filename || image->prepared || image->segments || image->jit_active ||
        image->data || image->fixed || image->exports.libraries ||
        image->tls.data || image->tls.filesz || image->tls.memsz) return -1;
    image->module = module;

    LOG("Opening '%s' for reading.", filename);
    if (util_load_file(filename, &data, &size) != 0) {
        LOG("Cannot load file.");
        return -1;
    }

    image->source_tag = load_source_tag(data, size);
    ret = load_detect_format(data, size) == LOAD_FORMAT_INVALID ?
        load_vpk(image, data, size, &entry) : load_executable(image, data, size, &entry);
    if (ret == 0) {
        int patched=guest_tp_plan_file(image->segments, image->segment_count,
                                      image->source_tag, size, filename);
        if (patched < 0) {
            LOG("Thread-pointer patch plan failed validation.");
            Result close=load_image_close(image);
            if (R_FAILED(close)) LOG("Patch-plan rollback failed: 0x%x", close);
            ret=-1;
        } else if (patched) {
            image->guest_tp_retargeted=true;
            LOG("Experimental guest thread-pointer plan applied: %d sites", patched);
        }
        if (ret==0) {
            size_t length=strnlen(filename,sizeof(image->module_info.path)-1);
            memcpy(image->module_info.path,filename,length);
            image->module_info.path[length]=0;
        }
    }
    if (ret < 0) LOG("Unsupported file or executable load failed.");

    free(data);
    return ret;
}

int load_file_prepare(LoadImage *image, const char *filename)
{
    return load_file_prepare_kind(image, filename, false);
}

int load_module_prepare(LoadImage *image, const char *filename)
{
    return load_file_prepare_kind(image, filename, true);
}

static int load_vpk(LoadImage *image, const void *data, uint32_t size, void **entry)
{
    mz_bool status;
    mz_zip_archive zip_archive;
    size_t uncompressed_size;
    void *eboot_bin;
    int ret;

    mz_zip_zero_struct(&zip_archive);

    status = mz_zip_reader_init_mem(&zip_archive, data, size, 0);
    if (status == MZ_FALSE)
        return -1;

    LOG("Found a ZIP file (VPK), extracting eboot.bin.");

    eboot_bin =
        mz_zip_reader_extract_file_to_heap(&zip_archive, "eboot.bin", &uncompressed_size, 0);
    if (!eboot_bin) {
        mz_zip_reader_end(&zip_archive);
        return -1;
    }

    ret = load_executable(image, eboot_bin, uncompressed_size, entry);

    mz_free(eboot_bin);
    mz_zip_reader_end(&zip_archive);

    return ret;
}

static int load_executable(LoadImage *image, const uint8_t *data, size_t size, void **entry)
{
    *entry = NULL;
    switch (load_detect_format(data, size)) {
    case LOAD_FORMAT_ELF:
        return load_elf(image, data, size, entry);
    case LOAD_FORMAT_SELF:
        return load_self(image, data, size, entry);
    default:
        return -1;
    }
}

static int load_self(LoadImage *image, const void *data, size_t size, void **entry)
{
    const SCE_header *self_header = data;
    if (size < sizeof(*self_header) ||
        !load_file_range(size, self_header->elf_offset, sizeof(Elf32_Ehdr)) ||
        self_header->elf_offset % _Alignof(Elf32_Ehdr) != 0 ||
        self_header->phdr_offset % _Alignof(Elf32_Phdr) != 0)
        return -1;
    const Elf32_Ehdr *elf_hdr = (void *)((char *)data + self_header->elf_offset);
    if (elf_hdr->e_phentsize != sizeof(Elf32_Phdr) || elf_hdr->e_phnum == 0 ||
        !load_file_range(size, self_header->phdr_offset,
                         (uint64_t)elf_hdr->e_phnum * sizeof(Elf32_Phdr)) ||
        !load_file_range(size, self_header->section_info_offset,
                         (uint64_t)elf_hdr->e_phnum * sizeof(segment_info)))
        return -1;
    const Elf32_Phdr *prog_hdrs = (void *)((char *)data + self_header->phdr_offset);
    const segment_info *seg_infos = (void *)((char *)data + self_header->section_info_offset);
    segment_info_t *segments;
    const Elf32_Phdr *seg_header;
    uint8_t *seg_bytes;
    void *uncompressed;
    mz_ulong dest_bytes;
    int ret;

    if (self_header->magic != 0x00454353) {
        LOG("SELF is corrupt or encrypted. Decryption is not yet supported.");
        return -1;
    }

    if (self_header->version != 3) {
        LOG("SELF version 0x%" PRIx32 " is not supported.", self_header->version);
        return -1;
    }

    if (self_header->header_type != 1) {
        LOG("SELF header type 0x%x is not supported.", self_header->header_type);
        return -1;
    }

    LOG("Loading SELF: ELF type: 0x%x, header_type: 0x%x, self_filesize: 0x%llx, self_offset: "
        "0x%llx",
        elf_hdr->e_type, self_header->header_type, self_header->self_filesize,
        self_header->self_offset);

    /* Make sure it contains a PSVita ELF */
    if (elf_check_vita_header(elf_hdr) < 0) {
        LOG("Check header failed.");
        return -1;
    }

    segments = calloc(elf_hdr->e_phnum, sizeof(segment_info_t));
    if (!segments)
        return -1;

    for (Elf32_Half i = 0; i < elf_hdr->e_phnum; i++) {
        seg_header = &prog_hdrs[i];
        if ((seg_header->p_type == PT_LOAD || seg_header->p_type == PT_LOOS) &&
            (seg_infos[i].encryption != 2 ||
             (seg_infos[i].compression != 1 && seg_infos[i].compression != 2) ||
             (seg_header->p_type == PT_LOAD && seg_header->p_filesz > seg_header->p_memsz) ||
             (seg_infos[i].compression == 2
                  ? !load_file_range(size, seg_infos[i].offset, seg_infos[i].length)
                  : !load_file_range(size, self_header->header_len, seg_header->p_offset) ||
                    !load_file_range(size, self_header->header_len + seg_header->p_offset,
                                      seg_header->p_filesz)))) {
            ret = -1;
            goto done;
        }
        seg_bytes = NULL;
        if ((seg_header->p_type == PT_LOAD || seg_header->p_type == PT_LOOS) &&
            seg_infos[i].compression == 1)
            seg_bytes = (uint8_t *)data + self_header->header_len + seg_header->p_offset;

        LOG("Segment %i: encryption: %lld, compression: %lld", i, seg_infos[i].encryption,
            seg_infos[i].compression);

        segments[i].p_type = seg_header->p_type;
        segments[i].p_filesz = seg_header->p_filesz;
        segments[i].p_memsz = seg_header->p_memsz;
        segments[i].p_flags = seg_header->p_flags;
        segments[i].p_align = seg_header->p_align;
        segments[i].p_vaddr = seg_header->p_vaddr;

        if (seg_header->p_type == PT_LOAD) {
            if (seg_header->p_memsz != 0) {
                if (seg_infos[i].compression == 2) {
                    dest_bytes = seg_header->p_filesz;
                    uncompressed = malloc(seg_header->p_memsz);
                    if (!uncompressed) {
                        ret = -1;
                        goto done;
                    }

                    ret = mz_uncompress(uncompressed, &dest_bytes,
                                        (uint8_t *)data + seg_infos[i].offset, seg_infos[i].length);
                    if (ret != MZ_OK || dest_bytes != seg_header->p_filesz) {
                        free(uncompressed);
                        ret = -1;
                        goto done;
                    }

                    segments[i].src_data = uncompressed;
                    segments[i].needs_free = true;
                } else {
                    segments[i].src_data = seg_bytes;
                }
            }
        } else if (seg_header->p_type == PT_LOOS) {
            if (seg_infos[i].compression == 2) {
                dest_bytes = seg_header->p_filesz;
                uncompressed = malloc(seg_header->p_filesz);
                if (!uncompressed) {
                    ret = -1;
                    goto done;
                }

                ret = mz_uncompress(uncompressed, &dest_bytes,
                                    (uint8_t *)data + seg_infos[i].offset, seg_infos[i].length);
                if (ret != MZ_OK || dest_bytes != seg_header->p_filesz) {
                    free(uncompressed);
                    ret = -1;
                    goto done;
                }

                segments[i].src_data = uncompressed;
                segments[i].needs_free = true;
            } else {
                segments[i].src_data = seg_bytes;
            }
        } else {
            LOG("Unknown segment type 0x%" PRIx32, seg_header->p_type);
        }
    }

    ret = load_segments(image, entry, elf_hdr->e_entry, segments, elf_hdr->e_phnum, elf_hdr->e_type);

done:
    for (Elf32_Half i = 0; i < elf_hdr->e_phnum; i++) {
        if (segments[i].needs_free)
            free(segments[i].src_data);
    }

    free(segments);

    return ret;
}

static int load_elf(LoadImage *image, const void *data, size_t size, void **entry)
{
    const Elf32_Ehdr *elf_hdr = data;
    Elf32_Phdr *prog_hdrs;
    int ret;

    if (size < sizeof(*elf_hdr) || elf_hdr->e_phentsize != sizeof(Elf32_Phdr) ||
        elf_hdr->e_phoff % _Alignof(Elf32_Phdr) != 0 ||
        !load_file_range(size, elf_hdr->e_phoff,
                         (uint64_t)elf_hdr->e_phnum * sizeof(Elf32_Phdr)))
        return -1;

    /* Make sure it's a PSVita ELF */
    if (elf_check_vita_header(elf_hdr) < 0) {
        LOG("Check header failed.");
        return -1;
    }

    LOG("Found %u program segments.", elf_hdr->e_phnum);
    if (elf_hdr->e_phnum < 1) {
        LOG("No program sections to load!");
        return -1;
    }

    /* Read ELF program headers */
    LOG("Reading program headers.");
    prog_hdrs = (void *)((uintptr_t)data + elf_hdr->e_phoff);

    segment_info_t *segments = calloc(elf_hdr->e_phnum, sizeof(segment_info_t));
    if (!segments)
        return -1;

    for (Elf32_Half i = 0; i < elf_hdr->e_phnum; i++) {
        if (!load_file_range(size, prog_hdrs[i].p_offset, prog_hdrs[i].p_filesz)) {
            free(segments);
            return -1;
        }
        segments[i].src_data = (char *)data + prog_hdrs[i].p_offset;
        segments[i].p_type = prog_hdrs[i].p_type;
        segments[i].p_filesz = prog_hdrs[i].p_filesz;
        segments[i].p_memsz = prog_hdrs[i].p_memsz;
        segments[i].p_flags = prog_hdrs[i].p_flags;
        segments[i].p_align = prog_hdrs[i].p_align;
        segments[i].p_vaddr = prog_hdrs[i].p_vaddr;
    }

    ret = load_segments(image, entry, elf_hdr->e_entry, segments, elf_hdr->e_phnum, elf_hdr->e_type);

    free(segments);

    return ret;
}

static int load_segments(LoadImage *image, void **entry, Elf32_Addr e_entry, segment_info_t *segments,
                         int num_segments, Elf32_Half type)
{
    uint32_t code_size = 0, data_size = 0;
    uint32_t code_offset = 0, data_offset = 0;
    uintptr_t data_addr = 0, code_rw_addr = 0, code_rx_addr = 0;
    uintptr_t addr_rw, addr_rx;
    sce_module_info_raw *mod_info;
    int mod_info_idx;
    Result res;

    Jit *jit = &image->jit;
    bool fixed = type == ET_SCE_EXEC;
    bool layout_conflict = false;
    if (fixed) {
        if (load_fixed_layout(segments, num_segments) != 0) {
            LOG("Unsupported or overlapping fixed-address segment layout.");
            return -1;
        }
        image->fixed = calloc(num_segments, sizeof(*image->fixed));
        if (!image->fixed) return -1;
        image->fixed_count = num_segments;
        /* Reserve the entire layout before selecting any temporary aliases. */
        for (int i = 0; i < num_segments; ++i) {
            if (segments[i].p_type != PT_LOAD || !segments[i].p_memsz) continue;
            res = load_fixed_reserve(&image->fixed[i], segments[i].p_vaddr,
                                    segments[i].p_memsz, (segments[i].p_flags & PF_X) != 0);
            if (R_FAILED(res)) {
                LOG("Fixed reservation at 0x%08" PRIx32 " failed: 0x%x", segments[i].p_vaddr, res);
                layout_conflict = res == MAKERESULT(Module_Libnx, LibnxError_AlreadyMapped);
                goto err_free_data;
            }
        }
    }
    if (!fixed) {
        /* Calculate total code and data sizes */
        for (int i = 0; i < num_segments; i++) {
            if (segments[i].p_type == PT_LOAD) {
                uint32_t alignment = segments[i].p_align;
                if ((alignment && (alignment & (alignment - 1))) ||
                    segments[i].p_filesz > segments[i].p_memsz)
                    return -1;
                uint32_t padding = alignment > 1 ? alignment - 1 : 0;
                uint32_t *total = (segments[i].p_flags & PF_X) ? &code_size : &data_size;
                if (segments[i].p_memsz > UINT32_MAX - padding ||
                    *total > UINT32_MAX - segments[i].p_memsz - padding)
                    return -1;
                *total += segments[i].p_memsz + padding;
            }
        }

        LOG("Total needed code size: 0x%" PRIx32, code_size);
        LOG("Total needed data size: 0x%" PRIx32, data_size);

        res = jitCreate(jit, code_size);
        if (R_FAILED(res)) {
            LOG("jitCreate failed: 0x%" PRIx32, res);
            return -1;
        }

        image->jit_active = true;
        image->data = malloc(data_size ? data_size : 1);
        data_addr = (uintptr_t)image->data;
        if (!data_addr) {
            LOG("Could not allocate buffer for the data segment");
            goto err_jit_close;
        }

        code_rw_addr = (uintptr_t)jitGetRwAddr(jit);
        code_rx_addr = (uintptr_t)jitGetRxAddr(jit);

        LOG("JIT code RW addr: %p", (void *)code_rw_addr);
        LOG("JIT code RX addr: %p", (void *)code_rx_addr);
        LOG("Data        addr: %p", (void *)data_addr);

        res = jitTransitionToWritable(jit);
        if (R_FAILED(res)) {
            LOG("Could not transition JIT to writable: 0x%" PRIx32, res);
            goto err_free_data;
        }
    }
    for (int i = 0; i < num_segments; i++) {
        if (segments[i].p_type == PT_LOAD) {
            if (!segments[i].p_memsz)
                continue;
            LOG("Found loadable segment (%u)", i);
            LOG("  p_filesz: 0x%" PRIx32, segments[i].p_filesz);
            LOG("  p_memsz:  0x%" PRIx32, segments[i].p_memsz);
            LOG("  p_align:  0x%" PRIx32, segments[i].p_align);

            if (fixed) {
                res = load_fixed_map(&image->fixed[i]);
                if (R_FAILED(res)) {
                    LOG("Fixed mapping at 0x%08" PRIx32 " failed: 0x%x", segments[i].p_vaddr, res);
                    goto err_free_data;
                }
                addr_rw = (uintptr_t)image->fixed[i].writable;
                addr_rx = segments[i].p_vaddr;
            } else if ((segments[i].p_flags & PF_X) == PF_X) {
                uint32_t offset;
                if (load_layout_place(code_rx_addr, code_offset, segments[i].p_memsz,
                                        segments[i].p_align, code_size, &offset, &code_offset) != 0)
                    goto err_free_data;
                addr_rx = code_rx_addr + offset;
                addr_rw = code_rw_addr + offset;
            } else {
                uint32_t offset;
                if (load_layout_place(data_addr, data_offset, segments[i].p_memsz,
                                        segments[i].p_align, data_size, &offset, &data_offset) != 0)
                    goto err_free_data;
                addr_rw = addr_rx = data_addr + offset;
            }

            segments[i].rx_addr = addr_rx;
            segments[i].rw_addr = addr_rw;

            if ((segments[i].p_flags & PF_X) == PF_X) {
                LOG("Code segment loaded at %p for RW, %p for RX", (void *)addr_rw,
                    (void *)addr_rx);
            } else {
                LOG("Data segment loaded at %p ", (void *)addr_rw);
            }

            memcpy((void *)segments[i].rw_addr, segments[i].src_data, segments[i].p_filesz);
            memset((char *)segments[i].rw_addr + segments[i].p_filesz, 0,
                   segments[i].p_memsz - segments[i].p_filesz);
        } else {
            LOG("Segment %u is not loadable. Skipping.", i);
            continue;
        }
    }

    /* Relocations can target later program headers, so every loadable segment
     * must have its final address before processing relocation records. */
    for (int i = 0; i < num_segments; i++) {
        if (segments[i].p_type == PT_SCE_RELA &&
            load_relocate(segments[i].src_data, segments[i].p_filesz, segments, num_segments) < 0)
            goto err_free_data;
    }

    /* Get module info */
    LOG("Getting module info.");
    if ((mod_info_idx = elf_get_sce_module_info(e_entry, segments, num_segments, &mod_info)) < 0) {
        LOG("Cannot find module info section.");
        goto err_free_data;
    }
    LOG("Module name: %.27s", mod_info->name);
    LOG("  export table offset: 0x%" PRIx32, mod_info->export_top);
    LOG("  import table offset: 0x%" PRIx32, mod_info->import_top);
    LOG("  tls start: 0x%" PRIx32, mod_info->tls_start);
    LOG("  tls filesz: 0x%" PRIx32, mod_info->tls_filesz);
    LOG("  tls memsz: 0x%" PRIx32, mod_info->tls_memsz);
    if (mod_info->tls_filesz > mod_info->tls_memsz ||
        (mod_info->tls_filesz &&
         (mod_info->tls_start > segments[mod_info_idx].p_filesz ||
          mod_info->tls_filesz > segments[mod_info_idx].p_filesz - mod_info->tls_start)) ||
        guest_tls_template_create(&image->tls,
            mod_info->tls_filesz ? (const char *)segments[mod_info_idx].rw_addr + mod_info->tls_start : NULL,
            mod_info->tls_filesz, mod_info->tls_memsz) < 0) {
        LOG("Invalid or unallocatable TLS initialization template.");
        goto err_free_data;
    }

    if (mod_info->export_top > mod_info->export_end ||
        mod_info->export_end > segments[mod_info_idx].p_memsz ||
        mod_info->export_top % 4 != 0) {
        LOG("Invalid module export range.");
        goto err_free_data;
    }
    ExportReader reader = {segments, num_segments};
    const void *export_records = (const void *)(segments[mod_info_idx].rw_addr + mod_info->export_top);
    uint32_t process_param = 0;
    if (load_exports_metadata(&image->lifecycle, &process_param, export_records,
            mod_info->export_end - mod_info->export_top, read_export_address, &reader) < 0 ||
        load_exports_register(&image->exports, image, export_records,
            mod_info->export_end - mod_info->export_top, read_export_address, &reader) < 0) {
        LOG("Invalid or unsupported guest library exports.");
        goto err_free_data;
    }
    if (!image->module && (load_process_parameters_read(&image->parameters,
            process_param, read_export_address, &reader) < 0 ||
        load_process_parameters_register(&image->exports, image,
            process_param, read_export_address, &reader) < 0)) {
        LOG("Invalid or unsupported process parameters.");
        goto err_free_data;
    }
    /* Resolve NIDs */
    if (mod_info->import_top > mod_info->import_end ||
        mod_info->import_end > segments[mod_info_idx].p_memsz || mod_info->import_top % 4 != 0) {
        LOG("Invalid module import range.");
        goto err_free_data;
    }
    /* Find the entry point (address belonging to the RX JIT area) */
    *entry = (void *)image->lifecycle.start;
    if (!*entry && mod_info->module_start != UINT32_MAX &&
        (!image->module || mod_info->module_start != 0)) {
        uint64_t address = (uint64_t)segments[mod_info_idx].rx_addr + mod_info->module_start;
        size_t bytes = (mod_info->module_start & 1) ? 2 : 4;
        if (address > UINT32_MAX || (address & ~UINT64_C(1)) % bytes ||
            (mod_info->module_start & ~1U) >= segments[mod_info_idx].p_memsz ||
            !read_export_address(&reader, (uint32_t)address & ~1U, bytes, true)) {
            LOG("Invalid module entry function.");
            goto err_free_data;
        }
        *entry = (void *)(uintptr_t)address;
        image->lifecycle.start = *entry;
    }
    LOG("Module lifecycle start=%p stop=%p exit=%p", image->lifecycle.start,
        image->lifecycle.stop, image->lifecycle.exit);

    size_t info_offset=(const char *)mod_info-(const char *)segments[mod_info_idx].src_data;
    const sce_module_info_raw *relocated_info=(const void *)(segments[mod_info_idx].rw_addr+info_offset);
    if (load_module_info_build(&image->module_info,relocated_info,segments,num_segments,
            mod_info_idx,&image->lifecycle,read_export_address,&reader)<0) {
        LOG("Invalid or unrepresentable module information.");
        goto err_free_data;
    }
    atomic_store(&image->module_state,SCE_KERNEL_MODULE_STATE_READY);

    image->segments = calloc(num_segments, sizeof(*image->segments));
    if (!image->segments) goto err_free_data;
    for (int i = 0; i < num_segments; ++i) {
        if (segments[i].p_type != PT_LOAD) continue;
        image->segments[i] = segments[i];
        image->segments[i].src_data = NULL;
        image->segments[i].needs_free = false;
    }
    image->segment_count = num_segments;
    image->imports = (void *)(segments[mod_info_idx].rw_addr + mod_info->import_top);
    image->imports_size = mod_info->import_end - mod_info->import_top;
    image->entry = *entry;
    image->prepared = true;

    return 0;

err_free_data:
err_jit_close:
    *entry = NULL;
    res = load_image_close(image);
    if (R_FAILED(res)) {
        LOG("Executable rollback failed: 0x%x", res);
        return -1;
    }

    return layout_conflict ? LOAD_LAYOUT_CONFLICT : -1;
}
