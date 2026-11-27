#ifndef TEST_SWITCH_H
#define TEST_SWITCH_H

/* Host substitutes. Optional pthread locks check caller ownership, not the
 * implementation of libnx synchronization primitives. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#define BIT(n) (1U << (n))
#ifdef TEST_THREADED_RWLOCK
#include <pthread.h>
typedef pthread_rwlock_t RwLock;
static inline void rwlockInit(RwLock *lock) { assert(pthread_rwlock_init(lock, NULL) == 0); }
static inline void rwlockReadLock(RwLock *lock) { assert(pthread_rwlock_rdlock(lock) == 0); }
static inline void rwlockReadUnlock(RwLock *lock) { assert(pthread_rwlock_unlock(lock) == 0); }
static inline void rwlockWriteLock(RwLock *lock) { assert(pthread_rwlock_wrlock(lock) == 0); }
static inline void rwlockWriteUnlock(RwLock *lock) { assert(pthread_rwlock_unlock(lock) == 0); }
#else
typedef int RwLock;
static inline void rwlockInit(RwLock *lock) { *lock = 0; }
static inline void rwlockReadLock(RwLock *lock) { (void)lock; }
static inline void rwlockReadUnlock(RwLock *lock) { (void)lock; }
static inline void rwlockWriteLock(RwLock *lock) { (void)lock; }
static inline void rwlockWriteUnlock(RwLock *lock) { (void)lock; }
#endif
typedef int UEvent;
#ifdef TEST_THREAD_RUNTIME
#ifdef TEST_THREAD_RUNTIME_PTHREAD
#include <pthread.h>
typedef struct { void (*entry)(void *); void *argument; pthread_t native;
                 bool started, joined; } Thread;
#else
typedef struct { void (*entry)(void *); void *argument; } Thread;
#endif
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
#ifndef TEST_KERNEL_TIME
static inline uint64_t armGetSystemTick(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
static inline uint64_t armTicksToNs(uint64_t ticks) { return ticks; }
#endif
#else
typedef int Mutex;
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
#elif defined(TEST_PROCESS_EXIT)
void __attribute__((noreturn)) threadExit(void);
#else
static inline void threadExit(void) { abort(); }
#endif

#ifdef TEST_FIXED_MEMORY
typedef uint32_t Result;
typedef uint32_t Handle;
typedef uint32_t u32;
typedef uint64_t u64;
typedef struct { uint64_t addr, size; uint32_t type; } MemoryInfo;
typedef struct VirtmemReservation { uintptr_t address; size_t size; } VirtmemReservation;
typedef struct { int unused; } Jit;
#define INVALID_HANDLE 0
#define CUR_PROCESS_HANDLE 0xffff8001
#define R_FAILED(value) ((value) != 0)
#define R_SUCCEEDED(value) ((value) == 0)
#define MAKERESULT(module, description) ((Result)(((description) << 9) | (module)))
enum { Module_Libnx = 345, LibnxError_BadInput = 11, LibnxError_JitUnavailable = 35,
       LibnxError_OutOfMemory = 2, LibnxError_AlreadyMapped = 3,
       InfoType_AliasRegionAddress = 2, InfoType_AliasRegionSize = 3,
       InfoType_HeapRegionAddress = 4, InfoType_HeapRegionSize = 5,
       MemType_Unmapped = 0, Perm_Rw = 3, Perm_Rx = 5,
       CodeMapOperation_MapOwner = 0, CodeMapOperation_MapSlave = 1,
       CodeMapOperation_UnmapOwner = 2, CodeMapOperation_UnmapSlave = 3 };
bool envIsSyscallHinted(unsigned);
Result svcQueryMemory(MemoryInfo *, u32 *, uint64_t);
Result svcGetInfo(uint64_t *, u32, Handle, uint64_t);
Result svcCreateCodeMemory(Handle *, void *, size_t);
Result svcControlCodeMemory(Handle, u32, uint64_t, uint64_t, u32);
Result svcCloseHandle(Handle);
void virtmemLock(void);
void virtmemUnlock(void);
VirtmemReservation *virtmemAddReservation(void *, size_t);
void virtmemRemoveReservation(VirtmemReservation *);
void *virtmemFindCodeMemory(size_t, size_t);
void armDCacheFlush(void *, size_t);
void armICacheInvalidate(void *, size_t);
Result jitClose(Jit *);
#endif

#endif
