#ifndef TEST_SWITCH_H
#define TEST_SWITCH_H

/* Single-threaded host substitutes; these do not test libnx synchronization. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#define BIT(n) (1U << (n))
typedef int RwLock;
typedef int UEvent;
#ifdef TEST_THREAD_RUNTIME
typedef struct { void (*entry)(void *); void *argument; } Thread;
#else
typedef int Thread;
#endif
#ifdef TEST_THREADED_MUTEX
#include <pthread.h>
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t CondVar;
typedef uint32_t Result;
#define R_FAILED(value) ((value) != 0)
static inline void mutexInit(Mutex *mutex) { assert(pthread_mutex_init(mutex, NULL) == 0); }
static inline void mutexLock(Mutex *mutex) { assert(pthread_mutex_lock(mutex) == 0); }
static inline void mutexUnlock(Mutex *mutex) { assert(pthread_mutex_unlock(mutex) == 0); }
static inline bool mutexTryLock(Mutex *mutex) { return pthread_mutex_trylock(mutex) == 0; }
static inline void condvarInit(CondVar *condition) { assert(pthread_cond_init(condition, NULL) == 0); }
static inline Result condvarWakeAll(CondVar *condition) { return pthread_cond_broadcast(condition); }
static inline Result condvarWaitTimeout(CondVar *condition, Mutex *mutex, uint64_t ns)
{
    if (ns == UINT64_MAX)
        return pthread_cond_wait(condition, mutex);
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    uint64_t total = (uint64_t)deadline.tv_nsec + ns;
    deadline.tv_sec += total / 1000000000;
    deadline.tv_nsec = total % 1000000000;
    int result = pthread_cond_timedwait(condition, mutex, &deadline);
    return result == ETIMEDOUT ? 0xEA01 : (Result)result;
}
static inline uint64_t armGetSystemTick(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
static inline uint64_t armTicksToNs(uint64_t ticks) { return ticks; }
#else
typedef int Mutex;
static inline void rwlockInit(RwLock *lock) { *lock = 0; }
static inline void rwlockReadLock(RwLock *lock) { (void)lock; }
static inline void rwlockReadUnlock(RwLock *lock) { (void)lock; }
static inline void rwlockWriteLock(RwLock *lock) { (void)lock; }
static inline void rwlockWriteUnlock(RwLock *lock) { (void)lock; }
static inline void mutexInit(Mutex *mutex) { *mutex = 0; }
static inline void mutexLock(Mutex *mutex) { *mutex = 1; }
static inline void mutexUnlock(Mutex *mutex) { *mutex = 0; }
static inline bool mutexTryLock(Mutex *mutex) { *mutex = 1; return true; }
#endif
static inline void ueventCreate(UEvent *event, bool state) { *event = state; }
static inline void ueventSignal(UEvent *event) { *event = 1; }
#ifdef TEST_THREAD_RUNTIME
typedef int32_t s32;
typedef int64_t s64;
typedef uint32_t Result;
#define R_FAILED(value) ((value) != 0)
Result threadCreate(Thread *, void (*)(void *), void *, void *, size_t, int, int);
Result threadStart(Thread *);
Result threadClose(Thread *);
int threadTlsAlloc(void (*)(void *));
void threadTlsFree(int);
void threadTlsSet(int, void *);
void *threadTlsGet(int);
typedef struct { Thread *thread; UEvent *event; } Waiter;
static inline Waiter waiterForThread(Thread *thread) { return (Waiter){.thread = thread}; }
static inline Waiter waiterForUEvent(UEvent *event) { return (Waiter){.event = event}; }
Result waitSingle(Waiter, uint64_t);
void svcSleepThread(s64);
void __attribute__((noreturn)) threadExit(void);
#else
static inline void threadExit(void) { abort(); }
#endif

#endif
