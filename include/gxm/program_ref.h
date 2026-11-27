#ifndef GXM_PROGRAM_REF_H
#define GXM_PROGRAM_REF_H
#include <stdbool.h>
#include <stdatomic.h>

typedef struct { atomic_uint count; } GxmProgramRef;
/* Callers must own a live reference while accessing the object. Initialize to
 * one on creation; this counter does not validate stale program pointers. */
int gxm_program_ref_add(GxmProgramRef *ref);
int gxm_program_ref_release(GxmProgramRef *ref, bool *destroy);
int gxm_program_ref_get(const GxmProgramRef *ref, unsigned int *count);
#endif
