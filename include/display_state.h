#ifndef DISPLAY_STATE_H
#define DISPLAY_STATE_H

#include <stdbool.h>
#include <psp2/display.h>

/* CPU-side display state. Never hold its lock while issuing GPU operations. */
void display_state_start(void);
void display_state_stop(void);
void display_state_tick(void);
bool display_state_snapshot(SceDisplayFrameBuf *framebuffer);
int display_state_set(const SceDisplayFrameBuf *framebuffer, SceDisplaySetBufSync sync);
int display_state_get(SceDisplayFrameBuf *framebuffer, SceDisplaySetBufSync sync);
int display_state_wait(bool since_set);
int display_state_wait_set_multi(unsigned int periods);

#endif
