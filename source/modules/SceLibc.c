#include <stdlib.h>
#include <string.h>

#include "module.h"

/* These C interfaces share the ARM32 ABI with newlib. FILE, locale, time,
 * process lifetime and C++ runtime interfaces require separate adapters.
 * NIDs come from the pinned vita-headers SceLibc database. */
EXPORT_FUNC(SceLibc, 0x775A0CB2, malloc)
EXPORT_FUNC(SceLibc, 0xE7EC3D0B, calloc)
EXPORT_FUNC(SceLibc, 0x006B54BA, realloc)
EXPORT_FUNC(SceLibc, 0x5B9BB802, free)
EXPORT_FUNC(SceLibc, 0x7205BFDB, memcpy)
EXPORT_FUNC(SceLibc, 0xAF5C218D, memmove)
EXPORT_FUNC(SceLibc, 0x6DC1F0D8, memset)
EXPORT_FUNC(SceLibc, 0x7747F6D7, memcmp)
EXPORT_FUNC(SceLibc, 0x2F3E5B16, memchr)
EXPORT_FUNC(SceLibc, 0x8AECC873, strlen)
EXPORT_FUNC(SceLibc, 0x1B58FA3B, strcmp)
EXPORT_FUNC(SceLibc, 0xE4299DCB, strncmp)
EXPORT_FUNC(SceLibc, 0x85B924B7, strcpy)
EXPORT_FUNC(SceLibc, 0x9F87712D, strncpy)
EXPORT_FUNC(SceLibc, 0x1434FA46, strcat)
EXPORT_FUNC(SceLibc, 0xFBA69BC2, strncat)
EXPORT_FUNC(SceLibc, 0xB9336E16, strchr)
EXPORT_FUNC(SceLibc, 0xCEFDD143, strrchr)
EXPORT_FUNC(SceLibc, 0x0D5200CB, strstr)
EXPORT_FUNC(SceLibc, 0x4203B663, strspn)
EXPORT_FUNC(SceLibc, 0x0E29D27A, strcspn)
EXPORT_FUNC(SceLibc, 0x68C307B6, strpbrk)
EXPORT_FUNC(SceLibc, 0x21493BE7, atoi)
EXPORT_FUNC(SceLibc, 0xA1DBEE9F, atol)
EXPORT_FUNC(SceLibc, 0x875994F3, atoll)
EXPORT_FUNC(SceLibc, 0x181827ED, strtol)
EXPORT_FUNC(SceLibc, 0xF34AE312, strtoul)
EXPORT_FUNC(SceLibc, 0x39B7E681, strtoll)
EXPORT_FUNC(SceLibc, 0xE0E12333, strtoull)
EXPORT_FUNC(SceLibc, 0xD500DE27, atof)
EXPORT_FUNC(SceLibc, 0x6CA88B08, strtod)
EXPORT_FUNC(SceLibc, 0xA7CBE4A6, qsort)
EXPORT_FUNC(SceLibc, 0xD1BC28E7, bsearch)
EXPORT_FUNC(SceLibc, 0x8E5A06C5, abs)
EXPORT_FUNC(SceLibc, 0xBCEA304B, labs)
EXPORT_FUNC(SceLibc, 0x9D2D17CD, llabs)

DECLARE_LIBRARY(SceLibc, 0xBE43BB07);
