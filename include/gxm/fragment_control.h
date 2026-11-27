#ifndef GXM_FRAGMENT_CONTROL_H
#define GXM_FRAGMENT_CONTROL_H
#include <stdbool.h>
#include <psp2/gxm.h>

typedef struct { bool front_disabled, back_disabled; } GxmFragmentControl;
/* Layout consumed by the pinned shader translator's binding 3. */
typedef struct {
    float back_disabled, front_disabled, writing_mask, use_raw_image, res_multiplier;
} GxmFragmentUniform;

int gxm_fragment_control_set(GxmFragmentControl *control, bool back, SceGxmFragmentProgramMode mode);
GxmFragmentUniform gxm_fragment_control_uniform(const GxmFragmentControl *control,
                                               bool two_sided, bool *color_enabled);
#endif
