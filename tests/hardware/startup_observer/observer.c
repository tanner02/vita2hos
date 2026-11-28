/* Owned diagnostic module: observe reviewed register-argument startup callbacks.
 * Never patch executable instructions or change a callback's return value. */
#include <stdint.h>
#include <stddef.h>
#ifndef OBSERVER_HOST_TEST
#include <psp2/io/fcntl.h>
#include <psp2/kernel/modulemgr.h>
#else
#include "observer_io.h"
#endif

#define LIMIT 64
#define HEADER 32
#define ROW 56
#define PLAN "app0:/startup-init.plan"
#define SOURCE "app0:/executable"
#define REPORT "app0:/startup-init-v3.txt"
typedef uint32_t (*callback)(uint32_t, uint32_t, uint32_t, uint32_t);
struct stage { volatile uint32_t *slot; callback original; char name[48]; };
static struct stage stages[LIMIT];
static uint32_t stage_count;
static unsigned char plan[HEADER + LIMIT * ROW];

static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int write_all(int fd, const char *p, unsigned n)
{
    while (n) {
        int r = sceIoWrite(fd, p, n);
        if (r <= 0 || (unsigned)r > n) return -1;
        p += r; n -= (unsigned)r;
    }
    return 0;
}
static int line(const char *event, const char *name, uint32_t value)
{
    char text[112]; unsigned n = 0;
    while (*event) text[n++] = *event++;
    text[n++] = ' ';
    while (*name) text[n++] = *name++;
    text[n++] = ' '; text[n++] = '0'; text[n++] = 'x';
    for (unsigned i = 0; i < 8; ++i) text[n++] = "0123456789abcdef"[(value >> (28-4*i)) & 15];
    text[n++] = '\n';
    int fd = sceIoOpen(REPORT, SCE_O_WRONLY | SCE_O_APPEND, 0);
    if (fd < 0) return -1;
    int result = write_all(fd, text, n);
    if (sceIoClose(fd) < 0) result = -1;
    return result;
}
static void restore(unsigned i);
static uint32_t invoke(unsigned i, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    restore(i);
    line("BEGIN", stages[i].name, i);
    uint32_t result = stages[i].original(a0,a1,a2,a3);
    line("END", stages[i].name, result);
    return result;
}
#define WRAP(N) static uint32_t wrap_##N(uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3) \
    { return invoke(N,a0,a1,a2,a3); }
#define EACH(M) \
 M(0) M(1) M(2) M(3) M(4) M(5) M(6) M(7) \
 M(8) M(9) M(10) M(11) M(12) M(13) M(14) M(15) \
 M(16) M(17) M(18) M(19) M(20) M(21) M(22) M(23) \
 M(24) M(25) M(26) M(27) M(28) M(29) M(30) M(31) \
 M(32) M(33) M(34) M(35) M(36) M(37) M(38) M(39) \
 M(40) M(41) M(42) M(43) M(44) M(45) M(46) M(47) \
 M(48) M(49) M(50) M(51) M(52) M(53) M(54) M(55) \
 M(56) M(57) M(58) M(59) M(60) M(61) M(62) M(63)
EACH(WRAP)
#define ENTRY(N) wrap_##N,
static callback const wrappers[LIMIT] = { EACH(ENTRY) };
static void restore(unsigned i)
{
    /* Dispatchers may later locate records by their original callback address.
     * Restore before calling the guest; do not clobber a concurrent replacement. */
    uint32_t expected=(uint32_t)(uintptr_t)wrappers[i];
    __atomic_compare_exchange_n(stages[i].slot,&expected,
        (uint32_t)(uintptr_t)stages[i].original,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
}


static int fingerprint(uint64_t *value)
{
    unsigned char buf[4096]; uint64_t hash = UINT64_C(14695981039346656037);
    int fd = sceIoOpen(SOURCE, SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    int r;
    while ((r = sceIoRead(fd, buf, sizeof(buf))) > 0) {
        if ((unsigned)r > sizeof(buf)) { r = -1; break; }
        for (int i = 0; i < r; ++i) { hash ^= buf[i]; hash *= UINT64_C(1099511628211); }
    }
    int closed = sceIoClose(fd);
    if (r < 0 || closed < 0) return -1;
    *value = hash; return 0;
}
static int contains(uint32_t base, uint32_t size, uint32_t address, uint32_t bytes)
{
    return size && base <= UINT32_MAX-size && address >= base &&
        bytes <= size && address-base <= size-bytes;
}
static int prepare(void)
{
    int fd = sceIoOpen(PLAN, SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    unsigned used = 0; int r = 0;
    while (used < sizeof(plan) && (r = sceIoRead(fd, plan+used, sizeof(plan)-used)) > 0) {
        if ((unsigned)r > sizeof(plan)-used) { r = -1; break; }
        used += (unsigned)r;
    }
    unsigned char extra;
    int tail = r < 0 ? -1 : sceIoRead(fd, &extra, 1);
    int closed = sceIoClose(fd);
    if (r < 0 || tail != 0 || closed < 0 || used < HEADER) return -2;
    if (plan[0]!='S' || plan[1]!='T' || plan[2]!='I' || plan[3]!='1') return -3;
    uint32_t count = u32(plan+12);
    if (!count || count > LIMIT || used != HEADER+count*ROW) return -4;
    uint64_t source;
    if (fingerprint(&source) || source != ((uint64_t)u32(plan+4) | (uint64_t)u32(plan+8)<<32)) return -5;
    uint32_t code = u32(plan+16), code_size = u32(plan+20);
    uint32_t data = u32(plan+24), data_size = u32(plan+28);
    /* Validate every record before dereferencing or publishing any slot. */
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char *row = plan+HEADER+i*ROW;
        uint32_t slot = u32(row), fn = u32(row+4);
        if ((slot & 3) || !contains(data,data_size,slot,4) || !(fn & 1) ||
            !contains(code,code_size,fn & ~1U,2)) return -6;
        unsigned n;
        for (n = 0; n < 48 && row[8+n]; ++n)
            if (row[8+n] < 32 || row[8+n] > 126) return -7;
        if (!n || n == 48) return -7;
        for (unsigned j = 0; j < i; ++j)
            if (slot == u32(plan+HEADER+j*ROW)) return -8;
    }
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char *row = plan+HEADER+i*ROW;
        if (*(volatile uint32_t *)(uintptr_t)u32(row) != u32(row+4)) return -9;
    }
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char *row = plan+HEADER+i*ROW;
        stages[i].slot = (volatile uint32_t *)(uintptr_t)u32(row);
        stages[i].original = (callback)(uintptr_t)u32(row+4);
        for (unsigned n = 0; n < 48; ++n) stages[i].name[n] = row[8+n];
    }
    stage_count = count;
    return 0;
}
int module_start(unsigned args, void *argp)
{
    if (args || argp || stage_count) return -1;
    int fd = sceIoOpen(REPORT, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd < 0) return -2;
    if (sceIoClose(fd) < 0) return -2;
    int result = prepare();
    if (line("GUARD", "startup_observer_v3", (uint32_t)result) || result) return -3;
    /* This module starts before the main executable and creates no threads. */
    __sync_synchronize();
    for (unsigned i = 0; i < stage_count; ++i)
        *stages[i].slot = (uint32_t)(uintptr_t)wrappers[i];
    __sync_synchronize();
    return 0;
}
