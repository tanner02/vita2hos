#ifndef GXM_VERTEX_INPUT_H
#define GXM_VERTEX_INPUT_H
#include <stdbool.h>
#include <stdint.h>

typedef struct GxmVertexInput {
    /* UINT32_MAX means an unreferenced attribute in a named shader. */
    uint32_t location;
    bool integer_input;
    /* Non-register arrays consume one native location per element. Zero is
     * the single-element fallback for stripped shaders and legacy callers. */
    uint32_t array_elements;
    /* Register-format inputs fetch unsigned bits using shader-declared widths,
     * bypassing guest attribute normalization and floating-point conversion. */
    bool register_format;
    uint32_t register_component_bytes;
    /* Unsigned fetch elements after array flattening and C10 byte packing. */
    uint32_t register_components;
} GxmVertexInput;
#endif
