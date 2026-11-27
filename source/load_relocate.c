#include <string.h>
#include "load_segment.h"
#include "sce-elf.h"
#include "log.h"

int load_relocate(const void *reloc, uint32_t size, const segment_info_t *segs, int count)
{
    SCE_Rel record;
    SCE_Rel *entry = &record;
    uint32_t pos;
    uint16_t r_code = 0;
    uint32_t r_offset;
    uint32_t r_addend;
    uint8_t r_symseg;
    uint8_t r_datseg;
    int32_t offset;
    uint32_t symval, loc_rx;
    uintptr_t loc_rw;
    uint32_t upper, lower, sign, j1, j2;
    uint32_t value = 0;

    if ((!reloc && size) || !segs || count < 0)
        return -1;
    pos = 0;
    while (pos < size) {
        // get entry
        if (size - pos < sizeof(uint32_t))
            return -1;
        memset(&record, 0, sizeof(record));
        memcpy(&record.r_type, (const char *)reloc + pos, sizeof(uint32_t));
        uint32_t record_size = SCE_REL_IS_SHORT(record) ? 8 : 12;
        if (size - pos < record_size)
            return -1;
        memcpy(&record, (const char *)reloc + pos, record_size);
        if (SCE_REL_IS_SHORT(*entry)) {
            r_offset = SCE_REL_SHORT_OFFSET(entry->r_short);
            r_addend = SCE_REL_SHORT_ADDEND(entry->r_short);
            pos += 8;
        } else {
            r_offset = SCE_REL_LONG_OFFSET(entry->r_long);
            r_addend = SCE_REL_LONG_ADDEND(entry->r_long);
            if (SCE_REL_LONG_CODE2(entry->r_long)) {
                LOG("Unsupported secondary relocation at 0x%" PRIx32, pos);
                return -1;
            }
            pos += 12;
        }

        // get values
        r_symseg = SCE_REL_SYMSEG(*entry);
        r_datseg = SCE_REL_DATSEG(*entry);
        r_code = SCE_REL_CODE(*entry);
        if (r_code == R_ARM_NONE)
            continue;
        if (r_datseg >= count || segs[r_datseg].p_type != PT_LOAD ||
            !segs[r_datseg].rw_addr || r_offset > segs[r_datseg].p_memsz ||
            sizeof(uint32_t) > segs[r_datseg].p_memsz - r_offset ||
            segs[r_datseg].rw_addr > UINTPTR_MAX - r_offset ||
            (r_symseg != 15 && (r_symseg >= count || segs[r_symseg].p_type != PT_LOAD)))
            return -1;
        symval = r_symseg == 15 ? 0 : (uint32_t)segs[r_symseg].rx_addr;
        loc_rw = segs[r_datseg].rw_addr + r_offset;
        loc_rx = (uint32_t)segs[r_datseg].rx_addr + r_offset;
        const bool thumb = r_code == R_ARM_THM_PC22 || r_code == R_ARM_THM_MOVW_ABS_NC ||
                           r_code == R_ARM_THM_MOVT_ABS;
        if (loc_rw % (thumb ? 2 : 4) != 0)
            return -1;

        // perform relocation
        // taken from linux/arch/arm/kernel/module.c of Linux Kernel 4.0
        switch (SCE_REL_CODE(*entry)) {
        case R_ARM_V4BX: {
            /* Preserve Rm and the condition code. Alter
             * other bits to re-code instruction as
             * MOV PC,Rm.
             */
            value = (*(uint32_t *)loc_rw & 0xf000000f) | 0x01a0f000;
        } break;
        case R_ARM_ABS32:
        case R_ARM_TARGET1: {
            value = r_addend + symval;
        } break;
        case R_ARM_REL32:
        case R_ARM_TARGET2: {
            value = r_addend + symval - loc_rx;
        } break;
        case R_ARM_THM_PC22: {
            upper = *(uint16_t *)loc_rw;
            lower = *(uint16_t *)(loc_rw + 2);

            /*
             * 25 bit signed address range (Thumb-2 BL and B.W
             * instructions):
             *   S:I1:I2:imm10:imm11:0
             * where:
             *   S     = upper[10]   = offset[24]
             *   I1    = ~(J1 ^ S)   = offset[23]
             *   I2    = ~(J2 ^ S)   = offset[22]
             *   imm10 = upper[9:0]  = offset[21:12]
             *   imm11 = lower[10:0] = offset[11:1]
             *   J1    = lower[13]
             *   J2    = lower[11]
             */
            sign = (upper >> 10) & 1;
            j1 = (lower >> 13) & 1;
            j2 = (lower >> 11) & 1;
            offset = r_addend + symval - loc_rx;

            if (offset < (int32_t)0xff000000 || offset >= (int32_t)0x01000000) {
                LOG("reloc 0x%" PRIx32 " out of range: 0x%08" PRIx32, pos, symval);
                return -1;
            }

            sign = (offset >> 24) & 1;
            j1 = sign ^ (~(offset >> 23) & 1);
            j2 = sign ^ (~(offset >> 22) & 1);
            upper = (uint16_t)((upper & 0xf800) | (sign << 10) | ((offset >> 12) & 0x03ff));
            lower =
                (uint16_t)((lower & 0xd000) | (j1 << 13) | (j2 << 11) | ((offset >> 1) & 0x07ff));

            value = ((uint32_t)lower << 16) | upper;
        } break;
        case R_ARM_CALL:
        case R_ARM_JUMP24: {
            offset = r_addend + symval - loc_rx;
            if (offset < (int32_t)0xfe000000 || offset >= (int32_t)0x02000000 ||
                (offset & 3) != 0) {
                LOG("reloc 0x%" PRIx32 " out of range: 0x%08" PRIx32, pos, symval);
                return -1;
            }

            offset >>= 2;
            offset &= 0x00ffffff;

            value = (*(uint32_t *)loc_rw & 0xff000000) | offset;
        } break;
        case R_ARM_PREL31: {
            offset = r_addend + symval - loc_rx;
            value = offset & 0x7fffffff;
        } break;
        case R_ARM_MOVW_ABS_NC:
        case R_ARM_MOVT_ABS: {
            offset = symval + r_addend;
            if (SCE_REL_CODE(*entry) == R_ARM_MOVT_ABS)
                offset >>= 16;

            value = *(uint32_t *)loc_rw;
            value &= 0xfff0f000;
            value |= ((offset & 0xf000) << 4) | (offset & 0x0fff);
        } break;
        case R_ARM_THM_MOVW_ABS_NC:
        case R_ARM_THM_MOVT_ABS: {
            upper = *(uint16_t *)loc_rw;
            lower = *(uint16_t *)(loc_rw + 2);

            /*
             * MOVT/MOVW instructions encoding in Thumb-2:
             *
             * i    = upper[10]
             * imm4 = upper[3:0]
             * imm3 = lower[14:12]
             * imm8 = lower[7:0]
             *
             * imm16 = imm4:i:imm3:imm8
             */
            offset = r_addend + symval;

            if (SCE_REL_CODE(*entry) == R_ARM_THM_MOVT_ABS)
                offset >>= 16;

            upper =
                (uint16_t)((upper & 0xfbf0) | ((offset & 0xf000) >> 12) | ((offset & 0x0800) >> 1));
            lower = (uint16_t)((lower & 0x8f00) | ((offset & 0x0700) << 4) | (offset & 0x00ff));

            value = ((uint32_t)lower << 16) | upper;
        } break;
        default: {
            LOG("Unknown relocation code %u at 0x%" PRIx32, r_code, pos);
            return -1;
        }
        case R_ARM_NONE:
            continue;
        }

        memcpy((char *)segs[r_datseg].rw_addr + r_offset, &value, sizeof(value));
    }

    return 0;
}
