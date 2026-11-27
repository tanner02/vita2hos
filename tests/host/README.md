# Host regression checks

Run on Linux or WSL with a C compiler that supports AddressSanitizer and
UndefinedBehaviorSanitizer:

```sh
git submodule update --init external/mlib external/dlmalloc external/vita-headers
sh tests/host/run.sh
```

The test compiles the production `SceLibKernel.c` module against the pinned
Vita headers and mlib dictionary implementation. Small libnx/deko3d stubs
allow it to run without the Nintendo Switch toolchain. It checks unknown,
closed, and wrong-type descriptors, file reads after seeks, absolute and
relative seek positions, and invalid seek arguments. Utility checks cover
binary and text round trips, empty and missing files, non-seekable input,
oversized sparse files, and write errors (including buffered errors on close).
The utility tests use Linux `/proc` and `/dev/full`. Build artifacts are
created in a temporary directory and removed on exit.

Module checks exercise function lookup and require pointer-bearing export
sections to remain writable in non-PIC objects, as required by Switch loader
relocation. The runner uses `readelf` from binutils for this check.

GXM checks exercise the production uniform-upload function, covering F32 and
F16 scalar/vector arrays, partial updates, array padding, invalid parameters,
and binary16 rounding and special values. The host compiler's `_Float16`
conversion provides an independent reference for 100,000 binary32 samples.
Integer and packed C10 uniform conversion remain unsupported.

SceLibc checks resolve the production library through its NIDs and exercise
allocation/reallocation/free, overlapping moves, numeric parsing and a sorting
callback. These host tests cannot prove the ARM32 ABI; the Switch build remains
the compile/link check, and console runs are needed to verify execution.

SceSysmem checks cover requested alignment, zero-filled memory, CDRAM alignment,
invalid and freed UIDs, interior/end address lookup, malformed options, mapping
failure cleanup, and 1,000 allocation/free cycles. GPU mapping is simulated;
these checks do not establish real GPU visibility or cache behavior.

SceLibm checks call float and double exports by NID, including trigonometry,
roots, powers, output-pointer arguments, mixed float/integer arguments, signed
zero, infinities and NaNs. They use the host math library; target newlib's
numerical edge behavior and the Vita calling convention need console checks.

Texture-state checks exercise linear descriptor initialization, zero-dimension
rejection, point/linear filter changes, supported address modes and safe aliases,
data-pointer updates, null pointers, and preservation of unrelated descriptor
bits. Mipmapped filters and border-ignore modes remain unsupported.

Texture-layout checks cover format/channel translation for uncompressed
normalized and floating-point textures, eight-pixel source row padding,
explicit stride decoding, and rejection of unsupported layouts without changing
the output. Compressed, packed, gamma-corrected and cube textures still need
support. Host checks cannot establish GPU sampling results.

Texture-staging checks verify rectangular Morton layouts against independent
recursive fixtures, non-power-of-two swizzled images, 32x32 tiled images,
1/2/4/8-byte texels, short row repacking, padding, allocation failures, and
explicit lifetime retirement. The renderer retains staging blocks through the
scene and waits for the GPU before releasing them at the next scene or context
destruction. This conservative synchronization can stall rendering; performance
and hardware cache visibility remain unverified.

Event-flag checks use real pthread mutexes, condition variables and threads to
exercise FIFO bit consumption, AND/OR matching, clear modes, timeouts,
cancellation, deletion with a blocked waiter and single-waiter rejection.
Priority attributes, openable flags and callback dispatch are unsupported.
The libnx condition-variable implementation and Switch scheduling remain
hardware validation items.

The remaining single-threaded checks do not validate Switch ABI details, locking,
concurrent close operations, or hardware behavior. A Switch build and console
testing are still required for those.

Executable layout checks cover actual arena-base alignment, successive segment
placement, ELF alignments 0/1, power-of-two validation, capacity limits, and
pointer/integer overflow. They exercise the production placement helper rather
than the complete ELF/SELF parser; file-bound and relocation-record validation
remain separate work.

Address-translation checks cover separate executable/write aliases, data
segments with a shared address, gaps, segment ends, and arithmetic overflow.
The import resolver now validates record sizes and maps NID/entry/stub ranges
through their owning segments; complete title import coverage remains unproven.

Executable-format checks cover every truncated length below a SELF header,
exact ELF/SELF magic matching, and file ranges at EOF and integer limits.
The production loader passes source lengths through VPK extraction and checks
tables/payload ranges before reading them. These helper tests do not establish
complete malformed-file safety; imports and relocation records need additional
validation and end-to-end fixtures.

ARM relocation checks exercise the production relocation routine with short
and long records, unaligned input streams, absolute/relative writes, ARM branch
patches, truncation, invalid segments/offsets, unknown/secondary relocations,
and branch range failures. Unsupported secondary relocations fail explicitly.

Thread-runtime checks use a simulated libnx thread to exercise missing IDs,
zeroed TLS, startup-failure cleanup and retry, duplicate-start rejection,
normal return status and timeout error translation. They do not establish real
thread scheduling or deletion concurrency. Reusing a completed thread requires
additional lifecycle support; the current wrapper rejects a second start.

Named condition checks use production synchronization code with pthread-backed
native waits. They cover typed UID lookup, FIFO and priority selection, targeted
and broadcast wakeups, mutex reacquisition, timeout in both wait phases, mutex
and condition deletion, recursive counts, and public/raw export NIDs. Named
waits share the existing lightweight reference policy: release/reacquire one
count, and return errors without reacquisition. These policies follow the pinned
Vita3K source and still need firmware comparison. Open/close-by-name, callback
waits and condition-info queries remain unimplemented.

Color-surface gamma checks cover NID dispatch, descriptor copies, resetting to
linear, unsupported selective GR/layout modes, and unchanged descriptors after
errors. Linear ABGR8 BGR mode selects an sRGB render attachment view while
load/store image copies retain their UNORM view. Draw probe v7 / visible cube v3
adds a console-only mid-gray RGB/alpha readback and a linear-reset pass. Native
sRGB encoding and destination blending remain unverified until console runs;
these host checks do not establish pixel accuracy. Surface clipping is pending.

GXM marker/heartbeat checks exercise library-scoped NID dispatch, null arguments,
literal format characters in tags, bounded 63-byte tag reads, and untouched
surface/sync objects. Marker calls emit push/set/pop annotations in Debug logs;
Release builds omit them. There is no Razor capture, GPU timestamp, or marker
stack query. Heartbeat follows the pinned reference's validated no-op behavior
and never signals/waits a rendering or display fence.

Mspace checks use the pinned public-domain dlmalloc backend with ONLY_MSPACES
and no mmap/morecore growth. Four game-required exports create/destroy heaps
and allocate/free inside the caller-owned pool. A serialized registry retains
allocation identities, rejects overlapping pools and ignores invalid/double or
wrong-heap frees. Tests cover bounded capacity, reuse/coalescing, live allocations
at destruction, pool re-creation, guard bytes and concurrent allocation/free.
Handles and allocated storage remain guest pool addresses; registry records
use native metadata allocations. Base alignment currently requires 8 bytes;
firmware edge cases, ARM execution, stats and additional mspace APIs are unverified
or unimplemented. Destroying a heap invalidates all its outstanding allocations;
callers must stop using them before destruction.

Thread signals use one auto-reset native event and one atomic pending bit per
guest thread. A registry read lock protects delivery against record deletion.
The runtime fixture checks retained/duplicate signals, invalid UIDs, timeout
translation and both public/raw wait entry points; the pthread startup fixture
adds 128 actual wait/send round trips and delivery concurrent with deletion.
The host auto-reset event is modeled with atomic state, not Horizon scheduling.
The public SDK documents the single-pending contract. Unknown argument zero is
supported; other values are rejected. Nonzero delay is currently interpreted
as microseconds, bypassed for a pending signal, and counted against the original
timeout deadline. Delay timing, timeout updates, dormant/completed targets and
firmware edge cases still need comparison. Callback waits are not implemented.
