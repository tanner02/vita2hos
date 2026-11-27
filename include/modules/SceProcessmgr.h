#ifndef SCE_PROCESSMGR_H
#define SCE_PROCESSMGR_H
#include "load_process.h"
/* Host lifecycle only: init before guest threads start, finish after all stop.
 * The loaded main image owns the exposed parameter bytes until finish. */
int SceProcessmgr_init(const LoadProcess *process);
void SceProcessmgr_finish(void);
#endif
