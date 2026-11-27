#include <stdint.h>
#include <string.h>
#include <switch.h>
#include "display_state.h"

static Mutex g_lock;
static CondVar g_changed;
static bool g_initialized, g_ready;
static uint64_t g_generation, g_vblank, g_last_set;
static SceDisplayFrameBuf g_framebuffer;

void display_state_start(void)
{
    mutexLock(&g_lock);
    if (!g_initialized) { condvarInit(&g_changed); g_initialized = true; }
    if (!g_ready) {
        ++g_generation;
        g_vblank = g_last_set = 0;
        memset(&g_framebuffer, 0, sizeof(g_framebuffer));
        g_framebuffer.size = sizeof(g_framebuffer);
        g_ready = true;
    }
    mutexUnlock(&g_lock);
}

void display_state_stop(void)
{
    mutexLock(&g_lock);
    if (g_ready) {
        g_ready = false;
        ++g_generation;
        condvarWakeAll(&g_changed);
    }
    mutexUnlock(&g_lock);
}

void display_state_tick(void)
{
    mutexLock(&g_lock);
    if (g_ready) { ++g_vblank; condvarWakeAll(&g_changed); }
    mutexUnlock(&g_lock);
}

bool display_state_snapshot(SceDisplayFrameBuf *framebuffer)
{
    mutexLock(&g_lock);
    bool ready = g_ready;
    if (ready) *framebuffer = g_framebuffer;
    mutexUnlock(&g_lock);
    return ready;
}

static bool valid_sync(SceDisplaySetBufSync sync)
{ return sync == SCE_DISPLAY_SETBUF_IMMEDIATE || sync == SCE_DISPLAY_SETBUF_NEXTFRAME; }

int display_state_set(const SceDisplayFrameBuf *framebuffer, SceDisplaySetBufSync sync)
{
    if (!valid_sync(sync)) return (int)SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
    if (framebuffer && framebuffer->size != sizeof(*framebuffer))
        return (int)SCE_DISPLAY_ERROR_INVALID_VALUE;
    mutexLock(&g_lock);
    if (!g_ready) { mutexUnlock(&g_lock); return (int)SCE_DISPLAY_ERROR_NO_PIXEL_DATA; }
    if (framebuffer) g_framebuffer = *framebuffer;
    else {
        memset(&g_framebuffer, 0, sizeof(g_framebuffer));
        g_framebuffer.size = sizeof(g_framebuffer);
    }
    g_last_set = g_vblank;
    mutexUnlock(&g_lock);
    return 0;
}

int display_state_get(SceDisplayFrameBuf *framebuffer, SceDisplaySetBufSync sync)
{
    if (!framebuffer) return (int)SCE_DISPLAY_ERROR_INVALID_ADDR;
    if (!valid_sync(sync)) return (int)SCE_DISPLAY_ERROR_INVALID_UPDATETIMING;
    if (framebuffer->size != sizeof(*framebuffer)) return (int)SCE_DISPLAY_ERROR_INVALID_VALUE;
    return display_state_snapshot(framebuffer) ? 0 : (int)SCE_DISPLAY_ERROR_NO_PIXEL_DATA;
}

int display_state_wait(bool since_set)
{
    mutexLock(&g_lock);
    uint64_t generation = g_generation;
    uint64_t target = (since_set ? g_last_set : g_vblank) + 1;
    while (g_ready && generation == g_generation && g_vblank < target) {
        if (R_FAILED(condvarWaitTimeout(&g_changed, &g_lock, UINT64_MAX))) {
            mutexUnlock(&g_lock);
            return (int)SCE_DISPLAY_ERROR_NO_PIXEL_DATA;
        }
    }
    int result = g_ready && generation == g_generation ? 0 : (int)SCE_DISPLAY_ERROR_NO_PIXEL_DATA;
    mutexUnlock(&g_lock);
    return result;
}
