#ifndef TEST_SWITCH_H
#define TEST_SWITCH_H

/* Single-threaded host substitutes; these do not test libnx synchronization. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#define BIT(n) (1U << (n))
typedef int RwLock;
typedef int Mutex;
typedef int UEvent;
typedef int Thread;
static inline void rwlockReadLock(RwLock *lock) { (void)lock; }
static inline void rwlockReadUnlock(RwLock *lock) { (void)lock; }
static inline void rwlockWriteLock(RwLock *lock) { (void)lock; }
static inline void rwlockWriteUnlock(RwLock *lock) { (void)lock; }
static inline void mutexInit(Mutex *mutex) { *mutex = 0; }
static inline void mutexLock(Mutex *mutex) { *mutex = 1; }
static inline void mutexUnlock(Mutex *mutex) { *mutex = 0; }
static inline bool mutexTryLock(Mutex *mutex) { *mutex = 1; return true; }
static inline void ueventCreate(UEvent *event, bool state) { *event = state; }
static inline void ueventSignal(UEvent *event) { *event = 1; }
static inline void threadExit(void) { abort(); }

#endif
