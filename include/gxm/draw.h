#ifndef GXM_DRAW_H
#define GXM_DRAW_H

#include <stddef.h>
#include <stdint.h>
#include <psp2/gxm.h>
#include <deko3d.h>

int gxm_translate_primitive(SceGxmPrimitiveType primitive, DkPrimitive *translated);
int gxm_stream_divisor(SceGxmIndexSource source, uint32_t *divisor);
int gxm_instanced_counts(unsigned int total, unsigned int wrap,
                         uint32_t *indices, uint32_t *instances);
int gxm_validate_vertex_layout(const SceGxmVertexAttribute *attributes, unsigned int attribute_count,
                                const SceGxmVertexStream *streams, unsigned int stream_count);

int gxm_validate_index_range(SceGxmIndexFormat format, uintptr_t address,
                             uintptr_t block_base, size_t block_size,
                             unsigned int count, uint32_t *offset);

#endif
