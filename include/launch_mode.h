#ifndef LAUNCH_MODE_H
#define LAUNCH_MODE_H
#include <stdbool.h>
/* Missing file selects normal execution. Errors preserve the output value. */
int launch_mode_file(const char *path,bool *dependencies_only);
#endif
