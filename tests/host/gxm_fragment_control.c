#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_fragment_control.c"

int main(void)
{
    assert(sizeof(GxmFragmentUniform) == 20);
    assert(offsetof(GxmFragmentUniform, back_disabled) == 0);
    assert(offsetof(GxmFragmentUniform, front_disabled) == 4);
    assert(offsetof(GxmFragmentUniform, res_multiplier) == 16);
    for (unsigned config = 0; config < 8; ++config) {
        bool front = config & 1, back = config & 2, two_sided = config & 4;
        GxmFragmentControl control = {0};
        assert(gxm_fragment_control_set(&control, false, front ? SCE_GXM_FRAGMENT_PROGRAM_DISABLED :
                                                                  SCE_GXM_FRAGMENT_PROGRAM_ENABLED) == 0);
        assert(gxm_fragment_control_set(&control, true, back ? SCE_GXM_FRAGMENT_PROGRAM_DISABLED :
                                                                 SCE_GXM_FRAGMENT_PROGRAM_ENABLED) == 0);
        bool color;
        GxmFragmentUniform uniform = gxm_fragment_control_uniform(&control, two_sided, &color);
        /* Independently describe which faces still run a color program. */
        bool front_colors = !front;
        bool back_colors = !(two_sided ? back : front);
        assert(color == (front_colors || back_colors));
        if (!color) {
            assert(uniform.front_disabled == 0 && uniform.back_disabled == 0);
        } else {
            assert(uniform.front_disabled == (front_colors ? 0 : 1));
            assert(uniform.back_disabled == (back_colors ? 0 : 1));
        }
        assert(uniform.res_multiplier == 1 && uniform.writing_mask == 0 && uniform.use_raw_image == 0);
        GxmFragmentControl saved = control;
        assert(gxm_fragment_control_set(&control, false, 123) != 0);
        assert(memcmp(&control, &saved, sizeof(control)) == 0);
        /* Disabling two-sided mode must not destroy the stored back choice. */
        gxm_fragment_control_uniform(&control, false, &color);
        assert(memcmp(&control, &saved, sizeof(control)) == 0);
    }
    assert(gxm_fragment_control_set(NULL, false, SCE_GXM_FRAGMENT_PROGRAM_ENABLED) != 0);
    puts("Fragment face selection and depth-only color suppression checks passed.");
    return 0;
}
