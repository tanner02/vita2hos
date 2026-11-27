#ifndef SCE_CTRL_H
#define SCE_CTRL_H

int SceCtrl_init(void);
/* Stop, wake blocked readers and join/close the native sampler. A failure
 * retains its thread record for retry; callers must not free live resources. */
int SceCtrl_finish(void);

#endif
