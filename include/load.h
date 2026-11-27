#ifndef LOAD_H
#define LOAD_H

#include <switch.h>
#include "load_fixed.h"

typedef struct {
    Jit jit;
    bool jit_active;
    void *data;
    LoadFixedMemory *fixed;
    int fixed_count;
    uint64_t source_tag;
} LoadImage;

enum { LOAD_LAYOUT_CONFLICT = -2 };

int load_file(LoadImage *image, const char *filename, void **entry);
Result load_image_close(LoadImage *image);

#endif
