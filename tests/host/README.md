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

Reader/writer lock checks cover simultaneous readers, exclusive writers, recursive
reads and optional recursive writes, FIFO reader batches, priority queues, timeouts
and deletion with blocked readers/writers. Waiters retain the deleted record until
removing themselves, and ownership is tracked by Vita UID. Public create/read/write
and raw/try/unlock/delete NIDs are checked. New readers may join an existing read
lock even with a queued writer; queued grants batch adjacent readers. Mixed-mode
upgrade/downgrade requests are rejected with RW_LOCK_RECURSIVE. This policy avoids
an unsafe ownership conversion; firmware equivalence for those requests and mixed
reader/writer queue ordering is unverified. Callback waits, cancellation, named
open/close, info queries, priority inheritance and owner-death handling are pending.

CPU-affinity queries now retain validated guest creation masks, publish the raw
mask in TLS word 9 and thread-info snapshots, and resolve mask zero to USER_ALL
for the query, following the pinned reference. Dependency startup threads inherit
the parent's configured guest mask. Tests cover explicit/default/self/deleted
UID queries, invalid masks, snapshots, TLS and dependency inheritance. This is
guest metadata only: native threadCreate still uses Horizon's default affinity.
No native core pinning or change-affinity API is implemented by this change;
processor identity and firmware edge cases remain unverified.

Power-tick checks resolve the Processmgr NID and verify one-shot Horizon user
activity reports for DEFAULT, translated native failures, and no native activity
for unsupported selective timer modes or invalid values. DEFAULT maps to
appletReportUserIsActive; it never enables persistent AutoSleepDisabled. Native
idle/screen behavior still needs console observation. Selective suspend/off/dim
ticks and power lock/unlock remain unsupported.

Launch-mode parsing checks absent-file normal execution, explicit normal and
dependencies-only values, LF/CRLF, bounded reads, embedded NULs, extra content,
real file errors and output preservation. Loader tests prove diagnostic startup
order, failed-dependency gating, one-shot invocation and main remaining READY.
The frontend holds diagnostic execution for manual closure even after startup
failure: module-start joins cannot establish that resident workers stopped.
This mode does not make unresolved imports implemented or prove a title boots.

Missing function imports now branch to a bounded pool of compiled native
wrappers. Each records UNSUPPORTED_CALL on its first execution, with library/NID,
import stub and raw caller return address. Missing APIs still return NO_FUNC_NID;
the wrappers preserve the old r1 high-word result, and do not implement unknown
float or aggregate return conventions. The pool has 1024 sites across live images;
exhaustion logs a warning and retains the old immediate error stub. Successful
image closure releases slots, while failed cleanup retains them. Tests cover
concurrent first use, site/owner isolation, reuse, exhaustion, binding instruction
encoding, legacy fallback and cleanup retry. Compiled ARM inspection checks r1
save/restore; native execution remains a hardware test requirement.

report-imports.py separates binding inventory from runtime first-use records.
Duplicates are log occurrences, not call counts; only the first caller per
traced import/lifetime is recorded. Missing records do not prove an API was unused.

Controller tests use a native pthread sampler driven by controlled timer ticks
and deterministic pad states. Peek returns up to 64 actual cached samples;
ReadBufferPositive waits for a fresh batch and consumes it. Both return newest
first, matching the pinned reference's ordering. A count-one reader consumes
older unrequested records so it cannot build an input backlog. Mode changes
apply to subsequent samples; buffered history is retained. Sampling uses an
independent approximately-60-Hz native waiter, so input does not depend on GPU
presentation completing. Exact firmware cadence/history policy is unverified.

Tests cover timestamps, bounds, mode/stick conversion, ring overrun, competing
readers, non-consuming peeks, blocked-read cancellation, shutdown/restart epochs,
thread create/start/wait/close failures and failed-cleanup retry ownership.
Native sampler cleanup has a one-second join bound and never closes a thread
without proof of exit. The bootstrap uses native process exit if that proof
fails. Extended ports/modes, negative reads, rapid fire and firmware-specific
analog response curves remain unsupported. Analog and analog-wide use the same
normalized native stick range. Host tests do not establish hardware input.
Native thread-close lifetime reference:
https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/thread.c .

Network lifecycle tests model the bootstrap-owned native socket provider and
NIFM service acquisition. They cover host-provider failure, Vita init parameters,
duplicate init, native service failure/retry, single-release ownership, native
connection-state translation, preserved outputs on failures, explicit unsupported
sysmodule loads, and process cleanup. NET module load tracks the available HLE
provider; it does not claim a complete Vita socket/resolver implementation.
The caller pool is validated but not used as Horizon BSD backing memory; native
socket buffers remain bootstrap-owned. Guest termination never closes netlog's
shared native socket device. NET and the separate NP_TROPHY lifecycle provider are loadable; other public module IDs fail
with FATAL and remain unloaded. Unload while NET/NetCtl is active is rejected
rather than invalidating live services; exact firmware unload policy is unverified.
No callbacks, adhoc mode, Vita sockets/resolvers, network info or native connection
requests are implemented here. State queries report native NIFM status, mapping
its connecting states to CONNECTING and preserving output on native failure.
The host tests do not establish successful networking on the Switch.

Thread shutdown tests retain a live pthread worker across attempted quiesce and
finish calls. They check that new create/start operations are blocked, live
thread deletion is rejected, TLS/registry state survives failed cleanup, native
termination is required after the software completion flag, and initialization
resets the shutdown gate. The bootstrap checks quiescence before GPU/TLS/image
cleanup. If workers remain, it logs PROCESS_EXIT_WITH_LIVE_GUEST_THREADS and
calls Horizon ExitProcess directly, leaving shared resources intact for process
reclamation. That native exit branch remains unverified on hardware. This does
not add cooperative cancellation, stop guest workers individually, or claim to
free every unused dormant-thread allocation before the Horizon process ends.

Trophy-library tests cover init/term, allocation failure, reserved-option rejection,
64 owned operation handles, UID isolation across slot reuse and reinitialization,
recorded abort state, concurrent create/abort/destroy, provider load state and
unload rejection while initialized. Trophy init allocates its operation registry;
it does not establish a PSN identity or claim a trophy context exists. Archive-backed contexts and game-icon reads are implemented below; progress,
unlocks and synchronization remain unsupported.
The 64-handle cap, non-null-option rejection and active-provider unload policy
are explicit local limits, not firmware-verified behavior. Abort currently marks
handle state; no asynchronous trophy operation is implemented or cancelled.

Trophy archive tests require OpenSSL development headers and libcrypto on the
host (for example libssl-dev on Ubuntu). The Switch build uses a portable streaming SHA-1 backend; the pinned libnx32
archive exposes declarations but omits the implementation. Host tests compare
that backend with OpenSSL across block/padding boundaries and streamed chunks.
Owned version-2/3 fixtures exercise stream hashing with the embedded digest
zeroed, exact-name lookup, partial reads, empty reads, corrupt payloads,
malformed sizes/counts/strides, unterminated or path-like names, duplicate names,
metadata/data range separation, 64-bit overflow and allocation failure.

The reader borrows immutable bytes and owns only its validated entry index.
It limits archives to 64 MiB / 4096 entries, accepts flat printable ASCII names,
and supports version 2/3 with 64-byte headers/records. Payload aliases are safe
for read-only access and are allowed. No files are extracted or modified.
SHA-1 checks archive integrity; publisher signatures/authenticity are not checked.
This helper backs trophy context creation and game-icon reads.
Layout references: the pinned Vita3K TRP reader and
https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Loader/TRP.h .
The size field at header offset 0x14 is record size, not a general table offset.
A private host run validated the user-owned version-2 archive's SHA-1 and all
107 entry ranges without extracting content or executing guest instructions.
This does not establish trophy-context registration or hardware SHA-1 execution.

Trophy contexts use Vita3K's pinned pugixml (MIT license), initialized with
`git -C external/Vita3K submodule update --init external/pugixml` after fetching
external/Vita3K. The host runner and CI build the same XML adapter with
exceptions/XPath disabled. UTF-8 configuration validation checks one root,
communication identity, bounded/unique IDs, declared groups, grades and hidden
flags; absent gid means base group zero. DTDs, embedded NULs hiding content,
malformed/oversized numeric IDs and allocation failure are rejected without
publishing output.

CreateContext reads only app0:/sce_sys/trophy/NPWRxxxxx_nn/TROPHY.TRP, checks its
SHA-1 and metadata identity, and publishes a unique UID after successful parsing.
Failures preserve caller output and release pending bytes/indexes. Contexts are
bounded to eight and 128 MiB total archive bytes. Destroy/term/process finish
release owned data. GetGameIcon requires a valid non-aborted operation handle,
supports size query, and copies exact archive bytes. A too-small buffer returns
INSUFFICIENT_BUFFER and the required size without copying; this local policy has
not been verified against firmware. Only zero context options, explicit NPWR
communication IDs and non-null signature pointers are supported. Publisher
signature authenticity is not checked; this offline resource context establishes
neither a PSN identity nor entitlement. No progress files, trophy unlocks or
synchronization are fabricated or written.

Owned context fixtures cover missing/corrupt archives, invalid configuration,
identity mismatch, allocation failure, duplicate/stale IDs, pool exhaustion,
icon size/query/copy/abort and full teardown. A private host run created a context
from the real archive: 84 trophies, four extra groups, and an 80429-byte game
icon. No files were modified and no guest instructions executed. ARM context
creation, progress APIs and game rendering still require hardware evidence.

Display-state tests use real pthread condition variables with controlled presenter
ticks. They verify next-vblank and last-set-relative waits, spurious-wake loops,
multiple waiters, ticks without a framebuffer, coherent framebuffer snapshots,
and shutdown/restart generations. GPU presentation, actual refresh timing and
firmware behavior are not simulated. The presenter supplies approximate 60 Hz
ticks when no framebuffer is available and presentation-driven ticks otherwise;
GPU stalls can still delay these ticks. Immediate versus next-frame application
and clearing the visible output when the framebuffer is disabled remain outside
this synchronization change.

Process-exit coordination tests retain a live main guest thread while a worker
signals process exit. The host main-entry wait returns after a bounded native
join (100 ms grace) or a native join error, preserving the process status and
thread/TLS ownership. Quiescence still rejects teardown until the test explicitly
releases and joins the worker and main threads. A normally terminating main is
also joined and deleted. These host tests exercise the bootstrap precondition;
they do not simulate Horizon whole-process termination.

Network host checks also call all six byte-order exports before initialization,
checking literal wire bytes, unsigned boundaries, round trips and NID dispatch.
These pure functions do not provide sockets, online authentication or callbacks.
