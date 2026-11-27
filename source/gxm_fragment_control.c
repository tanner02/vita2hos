#include "gxm/fragment_control.h"

int gxm_fragment_control_set(GxmFragmentControl *control, bool back, SceGxmFragmentProgramMode mode)
{
    if (!control) return SCE_GXM_ERROR_INVALID_POINTER;
    if (mode != SCE_GXM_FRAGMENT_PROGRAM_ENABLED && mode != SCE_GXM_FRAGMENT_PROGRAM_DISABLED)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (back) control->back_disabled = mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED;
    else control->front_disabled = mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED;
    return 0;
}

GxmFragmentUniform gxm_fragment_control_uniform(const GxmFragmentControl *control,
                                               bool two_sided, bool *color_enabled)
{
    bool front = control->front_disabled;
    bool back = two_sided ? control->back_disabled : front;
    *color_enabled = !(front && back);
    /* Killing all fragments also suppresses depth/stencil writes. For a
     * depth-only pass, leave both alive and disable native color writes. */
    return (GxmFragmentUniform){
        .front_disabled = *color_enabled && front ? 1.0f : 0.0f,
        .back_disabled = *color_enabled && back ? 1.0f : 0.0f,
        .res_multiplier = 1.0f,
    };
}
