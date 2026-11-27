#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sched.h>
#include <switch.h>
static atomic_uint waits;
static Result observed_wait(CondVar *condition, Mutex *mutex, uint64_t ns);
#define condvarWaitTimeout observed_wait
#include "../../source/display_state.c"
#undef condvarWaitTimeout
static Result observed_wait(CondVar *condition, Mutex *mutex, uint64_t ns)
{
    atomic_fetch_add(&waits, 1);
    return condvarWaitTimeout(condition, mutex, ns);
}
typedef struct { bool since_set; int result; atomic_bool done; } Waiter;
static void *waiter(void *argument)
{
    Waiter *w = argument;
    w->result = display_state_wait(w->since_set);
    atomic_store(&w->done, true);
    return NULL;
}
static void wait_until_blocked(unsigned target)
{
    for (unsigned i = 0; i < 1000000 && atomic_load(&waits) < target; ++i) sched_yield();
    assert(atomic_load(&waits) >= target);
    /* Crossing the state lock proves the waiter reached the actual wait. */
    mutexLock(&g_lock); mutexUnlock(&g_lock);
}
static pthread_t start_wait(Waiter *w, bool since_set)
{
    *w = (Waiter){ .since_set = since_set, .result = 123 };
    unsigned target = atomic_load(&waits) + 1;
    pthread_t thread;
    assert(pthread_create(&thread, NULL, waiter, w) == 0);
    wait_until_blocked(target);
    assert(!atomic_load(&w->done));
    return thread;
}
static void finish_wait(pthread_t thread, Waiter *w, int result)
{
    assert(pthread_join(thread, NULL) == 0);
    assert(atomic_load(&w->done) && w->result == result);
}
static void *writer(void *unused)
{
    (void)unused;
    for (unsigned i = 1; i < 10000; ++i) {
        SceDisplayFrameBuf fb = { .size = sizeof(fb), .base = (void *)(uintptr_t)i,
            .pitch = i + 1, .width = i + 2, .height = i + 3 };
        assert(display_state_set(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) == 0);
    }
    return NULL;
}
int main(void)
{
    assert(display_state_wait(false) == (int)SCE_DISPLAY_ERROR_NO_PIXEL_DATA);
    display_state_start();
    Waiter first, second;
    pthread_t a = start_wait(&first, false), b = start_wait(&second, true);
    unsigned target = atomic_load(&waits) + 2;
    mutexLock(&g_lock); condvarWakeAll(&g_changed); mutexUnlock(&g_lock);
    wait_until_blocked(target);
    assert(!atomic_load(&first.done) && !atomic_load(&second.done));
    display_state_tick(); /* No framebuffer is necessary to release waits. */
    finish_wait(a, &first, 0); finish_wait(b, &second, 0);
    assert(display_state_wait(true) == 0); /* Already past the last set. */

    SceDisplayFrameBuf fb = { .size = sizeof(fb), .base = (void *)1,
        .pitch = 2, .width = 3, .height = 4 };
    assert(display_state_set(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) == 0);
    a = start_wait(&first, true);
    /* Idempotent start and a get must not reset time or wake the waiter. */
    display_state_start();
    SceDisplayFrameBuf out = { .size = sizeof(out) };
    assert(display_state_get(&out, SCE_DISPLAY_SETBUF_IMMEDIATE) == 0);
    assert(memcmp(&out, &fb, sizeof(out)) == 0);
    assert(!atomic_load(&first.done));
    display_state_tick(); finish_wait(a, &first, 0);

    assert(display_state_set(NULL, SCE_DISPLAY_SETBUF_IMMEDIATE) == 0);
    assert(display_state_snapshot(&out) && !out.base);
    a = start_wait(&first, true);
    display_state_tick(); finish_wait(a, &first, 0);
    a = start_wait(&first, false);
    display_state_stop(); display_state_start();
    finish_wait(a, &first, (int)SCE_DISPLAY_ERROR_NO_PIXEL_DATA);

    assert(display_state_set(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) == 0);
    pthread_t producer;
    assert(pthread_create(&producer, NULL, writer, NULL) == 0);
    for (unsigned i = 0; i < 10000; ++i) {
        assert(display_state_snapshot(&out));
        assert(out.pitch == (uintptr_t)out.base + 1);
        assert(out.width == out.pitch + 1 && out.height == out.width + 1);
    }
    assert(pthread_join(producer, NULL) == 0);
    SceDisplayFrameBuf before = out;
    assert(display_state_get(&out, (SceDisplaySetBufSync)3) == (int)SCE_DISPLAY_ERROR_INVALID_UPDATETIMING);
    assert(memcmp(&out, &before, sizeof(out)) == 0);
    assert(display_state_get(NULL, 0) == (int)SCE_DISPLAY_ERROR_INVALID_ADDR);
    fb.size = 0;
    assert(display_state_set(&fb, 0) == (int)SCE_DISPLAY_ERROR_INVALID_VALUE);
    display_state_stop(); display_state_stop(); display_state_tick();
    assert(!display_state_snapshot(&out));
    assert(memcmp(&out, &before, sizeof(out)) == 0);
    puts("Display state: predicate waits, spurious wakes, pre-frame ticks, set-relative waits, coherent snapshots and shutdown generations passed.");
    return 0;
}
