#ifndef SCE_MODULEMGR_H
#define SCE_MODULEMGR_H
#include "load_process.h"
/* Initialize before guest threads start. The process/images stay alive until
 * every guest thread stops and finish removes this registry. */
int SceModulemgr_init(const LoadProcess *process);
void SceModulemgr_finish(void);
#endif
