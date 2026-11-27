#ifndef VITA2HOS_SCE_SYSMODULE_H
#define VITA2HOS_SCE_SYSMODULE_H
#include <psp2/sysmodule.h>
#include <stdbool.h>
void SceSysmodule_finish(void);
bool SceSysmodule_stub_id_loaded(SceSysmoduleModuleId id);
#endif
