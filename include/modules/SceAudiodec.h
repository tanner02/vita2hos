#ifndef MODULES_SCE_AUDIODEC_H
#define MODULES_SCE_AUDIODEC_H

/* Use the pinned public SDK declarations, including 32-bit external VAs. */
#include <psp2/audiodec.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Runtime shutdown only, after guest decoder users have quiesced. */
void SceAudiodec_finish(void);
#ifdef __cplusplus
}
#endif
#endif
