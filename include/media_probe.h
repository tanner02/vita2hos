#ifndef VITA2HOS_MEDIA_PROBE_H
#define VITA2HOS_MEDIA_PROBE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit diagnostic run: module presence is not decoder implementation. */
bool media_probe_active(void);
void media_probe_record(const char *format, ...) __attribute__((format(printf,1,2)));
void media_probe_finish(void);
#ifdef __cplusplus
}
#endif
#endif
