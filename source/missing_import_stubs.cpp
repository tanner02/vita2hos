#include <utility>
#include <cstddef>
#include "missing_imports.h"
namespace {
using Stub=uint64_t (*)(uint32_t,uint32_t,uint32_t,uint32_t);
template<std::size_t Index>
uint64_t stub(uint32_t first,uint32_t high,uint32_t third,uint32_t fourth)
{
    uintptr_t caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    uint32_t result=static_cast<uint32_t>(missing_import_report(Index,caller,first,high,third,fourth));
    /* ARM32 returns this through r0/r1: retain the old stub's untouched r1,
     * including for callers expecting a register-pair result. Unknown APIs
     * remain unsupported; this does not invent a float/aggregate return ABI. */
    return (uint64_t(high)<<32)|result;
}
struct StubTable {Stub pointers[MISSING_IMPORT_SLOTS];};
template<std::size_t... Index>
constexpr StubTable make_table(std::index_sequence<Index...>)
{return {{&stub<Index>...}};}
/* Volatile keeps this non-PIC function-pointer table writable for relocation.
 * Its initializer is constant; there is no dynamic constructor or allocation. */
static volatile StubTable table=make_table(std::make_index_sequence<MISSING_IMPORT_SLOTS>{});

}
extern "C" uintptr_t missing_import_stub_address(unsigned index)
{return index<MISSING_IMPORT_SLOTS ? reinterpret_cast<uintptr_t>(table.pointers[index]) : 0;}
