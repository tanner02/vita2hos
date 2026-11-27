#ifndef VITA2HOS_SCE_NET_H
#define VITA2HOS_SCE_NET_H
#include <stdbool.h>
/* The bootstrap owns the native socket device used by netlog. Guest lifecycle
 * borrows it and must never call socketExit on that shared device. */
void SceNet_set_host_ready(bool ready);
bool SceNet_host_ready(void);
bool SceNet_is_initialized(void);
void SceNet_finish(void);
#endif
