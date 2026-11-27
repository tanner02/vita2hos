## Offline depth-buffer GPU staging (2026-11-27)

- Added force-load/store mode getters and setters, with descriptor-bit preservation
  and NID dispatch checks. BeginScene snapshots the depth descriptor and stages
  linear DF32, S8, or separate DF32_S8 planes through an owned GPU buffer;
  EndScene reads the shadow image back and exports only active guest pixels.
- Copy-buffer rowLength and imageHeight are byte strides in the deko3d copy
  implementation. The native ZF32_X24S8 transfer uses eight bytes per sample:
  float-depth bits followed by a word containing the low-byte stencil value.
  The shadow depth image is uncompressed. This representation follows source
  format traits and still requires hardware confirmation.
- Unsupported tiled/packed formats and forced transfers with multisampling
  return an error before scene mutation. Guest spans are checked against logical
  Sysmem allocations; callers must retain those allocations through EndScene.
  Absent planes use background values on load and are skipped on store.
- Forced stores currently wait synchronously for GPU completion before CPU
  export. Fragment notifications and the scene fence follow export; scene
  submission now precedes fence signaling on all paths. Temporary storage is
  retained until queued GPU references retire.
- The ASan/UBSan host model checks byte strides, upload/draw/readback order,
  untouched row padding, absent stencil, completion timing, allocation failures,
  and staging ownership. ARM Debug and Release builds passed. These checks do
  not execute a GPU and do not establish commercial-title compatibility.
- No Switch files were changed and no hardware test was requested while the
  user was unavailable. A later owned GPU roundtrip probe is still required.

# Hardware observations

## Runtime 76a91883c32536f0db3713f05e809d90868b1060

- The user reported that the corrected standalone HOME title launched, then
  displayed the runtime's executable-loading error dialog.
- USB readback recovered `log.txt` and confirmed the guest executable matched
  the deployed SHA-256 `150f41d4ea661fa24492c973f74cfea02af271ed608898e0ecc34cb2e5992eb7`.
- The log reached JIT allocation, segment loading, and module `surface_probe`,
  then reported `Invalid module entry function.` No guest code ran.
- Inspection found the probe export configuration set `imagemodule: true`.
  Vita SDK intentionally emits no module-start entry and clears the runtime
  import range for that module type. This was a probe packaging error, not a
  missing SD file or evidence of a general loader-format incompatibility.
- The corrected `imagemodule: false` guest passed entry-point/import validation
  and was uploaded by itself. Readback SHA-256:
  `a8898d953e49835e398a1f62d0d5dd4e1e60f0d608f24f367001a6db626f9485`.
  Its hardware result is recorded below; the installed runtime was unchanged.

### Corrected surface probe: PASS

- USB readback subsequently recovered a complete `surface-probe.txt` ending
  with `PASS surface_probe`, plus the matching `76a9188` runtime log.
- The report records three successful A/B/A framebuffer pattern comparisons,
  completed notifications and a successful null-color/null-sync scene.
- The runtime log confirms module import resolution, guest execution, context
  and queue setup, color copies, and both guest/display threads returning zero.
- The app remained open because this freestanding guest returned from its
  entry rather than calling `sceKernelExitProcess`; the thread manager was
  still waiting for the process-exit event. Later probes explicitly request exit.
- This validates standalone title launch, guest loading, surface copies and
  notification handling on this Switch. The probe performs no draw calls, so
  shader, depth/stencil draw behavior and commercial games remain unverified.

### Shader/depth/stencil draw probe: PASS

- USB readback recovered `draw-probe.txt` ending with `PASS draw_probe`, and a
  matching runtime log identifying the installed `76a9188` build.
- Guest SHA-256 at deployment:
  `ec6e61ba4c77a5e72dc8c81cb6dfc168b85791bf0348b26e4547380741b3bd69`;
  source commit `820c759a734417c8a334359d8255b28ffd48b2ce`.
- All surface checks passed again, followed by `PASS shader_red`,
  `PASS depth_only_occlusion`, and `PASS stencil_replace`.
- The runtime log confirms successful Vita GXP-to-GLSL translation, UAM
  compilation, indexed draws and color readback. It ends with process exit
  status zero, runtime shutdown and a successful `jitClose()`.
- This is direct hardware evidence for the tested shader/uniform/vertex,
  depth-only, stencil and readback paths. It does not cover textures, every
  primitive/state combination, or commercial-title compatibility.

### Texture probe version 2: invalid expected row order

- Guest source `84ff78bcbbfba08eb9ffcc580e0b99b231a98df7`, SHA-256
  `1cd43cfc0043b5e36307e135b8a4a066bf9dbd38180435008afc72ae44cb7ea9`.
- USB readback recovered `draw_probe v2` with the surface, shader, depth-only
  and stencil checks passing again. The first texture sample at pixel `0x410`
  read `0xffff0000` where the test expected `0xff0000ff`; the guest exited 1.
- The test incorrectly assumed increasing NDC Y maps downward. The default
  GXM viewport uses negative Y scale, while the fixture forwards UV unchanged.
  The observed blue sample matches the bottom-left texture quadrant at that
  framebuffer position. Version 3 corrects the expected mapping and adds a
  positive-Y viewport case. Its full texture result remains unverified.

### Texture probe version 3: sampling/update PASS, positive Y FAIL

- Guest source `064b0adbf61942703f816eb1d95539b68182a9da`, SHA-256
  `dcba44e256791addc13dc76fa7153ac64aad68f27ff2e4a513d5294aff20e637`.
- USB readback recovered `PASS texture_quadrants` and `PASS texture_update`,
  following all earlier surface/draw checks. These establish basic point-filtered
  sampling and re-upload of a changed guest allocation on the installed runtime.
- The positive-Y viewport case read `0xffffffff` at pixel `0x410` instead of
  `0xffff0000`. The complete probe failed and the guest exited 1. This case
  was unresolved in this run; version 4 adds coverage/axis diagnostics without changing
  the runtime or weakening the expected result.

### Texture probe version 4: explicit viewport calls produce no coverage

- Guest source `120e92d993fe43da08f18a18e91d9cedc21d728a`, SHA-256
  `bac36e8af23cda5d33791c2a821bd75c3e89a893d864978c8fa0683716cdc20e`.
- Default-viewport texture quadrants and texture updates passed again.
- All four samples retained the distinctive `0xff332211` background in each
  scene with an explicit viewport call, including restoring the default signs.
- Inspection of the built guest shows the SDK's `sceGxmSetViewport` shim moves
  six float arguments into s0-s5 and reuses r1-r3 for stack arguments before
  branching to the imported stub. The runtime implementation expected base-PCS
  core/stack arguments, so it received the wrong transform. This explains why
  explicit calls fail while the runtime's internal default setter works.
- A runtime fix exports VFP adapters for viewport and background depth
  getter/setter. Version 5 confirmed this fix below and adds
  an explicit background-depth return-value and descriptor check as well.

## Runtime 38972edc42260a53fc141c204e1dc750194a5bb7

- Standalone HOME-menu title reinstalled with the GXM scalar-float VFP adapters;
  USB readback log identifies `38972ed` and successful process exit/shutdown.
- Version 5 guest deployed SHA-256:
  `a8528f1344bea8ec8db3e09776e75dada71dc6f2cf3b89bafc692c0a088a5bdc`.
- `draw_probe v5` ends with `PASS draw_probe`, with no `FAIL` records.
- All earlier surface, shader, depth-only and stencil checks passed. The new
  background-depth getter/setter roundtrip passed. Texture quadrants, changed
  guest image upload, positive Y, both reversed axes, negative X, and restoring
  the default viewport all passed their exact pixel comparisons.
- This is direct hardware confirmation of the VFP export adapters and these
  rendering cases. Visible cube presentation and commercial-title boot remain
  separate requirements; neither is established by this offscreen report.

### Visible textured cube: rendering PASS, shutdown FAIL

- Guest source `ced41b378e35aff9521c9e4bdaca9c8348da5e4e`, SHA-256
  `970925b69b4f49c9f0a33ac4a4f203ddf46bcc4419dc1cd7da9e9287e7c29630`.
- USB deployment verified the previous passing version 5 guest, preserved it
  under a backup name, then verified the new executable by readback checksum.
  The installed runtime remains `38972ed`; no NSP reinstall was performed.
- User reported that the cube appeared, followed by a deko3d failure after some
  time. USB readback confirms all prerequisite checks, 120/240/360 frame markers,
  `PASS cube_frames`, and final `PASS visible_cube`.
- The runtime log confirms guest exit status 0. During shutdown, `dkDeviceDestroy`
  then fails with `unfreed memory blocks` (result 9). The guest deliberately
  retains its displayed allocations until the presenter stops, but the runtime
  had no Sysmem teardown to release their GPU mappings before device destruction.
- Sysmem teardown now releases outstanding guest mappings after display shutdown.
  Host regression tests cover retained and explicitly freed allocations; hardware
  confirmation of clean shutdown is recorded below. The guest PASS marker by itself
  does not cover runtime shutdown.

## Runtime 06ded1bed69ab6451cb373198d002451a27f5b35

- USB readback identifies the installed `06ded1b` runtime and the same visible
  cube guest. Its report reaches all 360 frames and `PASS visible_cube` again.
- The log ends with guest/process status 0, return from launch, and successful
  `jitClose()`. There is no deko3d fatal error after the presenter stops.
- This confirms the retained-memory cleanup fix on this test and provides a
  complete visible 3D run with clean runtime shutdown. It does not establish
  commercial-title compatibility.

## Runtime f5ea659bed25ceda8ccf5eb21c5ae11b598297d1

- Fixed-address probe SHA-256:
  `ea59fcf2675f9a786613048e11a646875379b40d703c73564358171fd210bdc5`.
  The user reported a loading/runtime error. USB readback shows reservation at
  `0x81000000` failed with libnx BadInput `0x1759`, before guest execution.
- A separate relocatable diagnostic linked only Horizon's query wrappers from
  the pinned libnx32 archive. Guest SHA-256:
  `aaecc3d059a58d91f1bed2807d57fef6a07a5e834576ff8e84996cb41399f2d0`.
  All memory/region queries succeeded, the report reached `DONE address_probe`,
  and the runtime exited with status 0 and successful image cleanup.
- In that diagnostic process, both `0x81000000` and `0x81879000` were in memory
  type 5 (heap), with read/write permissions. The complete heap reservation was
  `0x5ca00000` + `0x40000000`; the alias reservation was `0xad200000` + `0x40000000`.
- This explains the host address conflict, not a guest code failure. The runtime
  now checks reserved regions explicitly and has bounded standalone-title layout
  retries. Fixed-address execution was subsequently confirmed below.

## Host-only LittleBigPlanet table replay after ade07aa

- Input: private plaintext SELF working copies of the supplied PCSF00516 main
  executable and bundled libc, FIOS2, and Ult libraries. No target instructions
  were executed; the Switch was not changed.
- A static inventory found 6,124, 5,468, and 3,839 format-0 relocation records in
  libc, FIOS2, and Ult respectively. Their explicit relocation codes were
  ABS32 and Thumb MOVW/MOVT; the main fixed-address executable had no relocation
  segments. The inventory alone does not validate instruction targets.
- A host harness replayed the runtime's actual relocation and export-decoding
  C sources under ASan/UBSan, using allocated writable buffers and simulated
  32-bit guest addresses. All four images passed after the decoder correction.
- The initial replay rejected libc because its public libraries include trailing
  TLS exports. Ordinary exports can now register without publishing those TLS
  entries; complete table bounds remain checked. Any TLS imports are explicitly
  rejected, rather than being silently ignored.
- Decoded ordinary public symbols: libc 1,441 across three libraries, FIOS2 149,
  and Ult 62. All three libraries expose a startup function; FIOS2 and Ult also
  expose stop functions. In particular, a zero metadata start offset does not
  establish that FIOS2 lacks an exported startup entry.
- This checks relocated tables only. It does not prove import binding, native
  mappings, TLS, startup behavior, or commercial game boot. Private binaries and
  replay inputs are not included in the repository.

## Host-only cross-image binding replay after cd7b621

- The replay prepared the same four private plaintext images, published their
  ordinary exports into one registry, and invoked the runtime's import binder
  under ASan/UBSan. Guest addresses were simulated; no target code ran.
- Main completed mechanical binding, including its eight variable imports.
  Libc initially failed on `__sce_libcparam` (library `0x5ad9c136`, variable
  `0xdf084dfa`). The loader now obtains that export from the main executable's
  validated process-parameter structure and publishes the actual guest address
  of its libc parameters. Main, libc, and FIOS2 then completed mechanical binding.
- Ult still fails on the absent `_pLibPerfCaptureFlagPtr` export from ScePerf
  (library `0x447f047d`, variable `0x936a5f31`). This is an unresolved dependency,
  not evidence of a startup or runtime failure.
- The harness intentionally omits native HLE providers, leaving error-return
  stubs for unresolved functions. Its function-stub counts therefore do not
  measure the actual runtime's missing-function count. Successful mechanical
  binding does not demonstrate usable functions, TLS, initialization, or boot.

## Host-only native ScePerf variable binding after 9481d0e

- Added a native variable descriptor for `_pLibPerfCaptureFlagPtr`, whose stored
  value points to a 32-bit capture flag initialized to zero. This follows the
  pointer/flag layout in the bundled Vita3K ScePerf source. Capture remains
  inactive; profiling and capture-control APIs are not implemented.
- The four-image host replay now completes mechanical import binding for main,
  libc, FIOS2, and Ult under ASan/UBSan. The harness substitutes a simulated
  32-bit address for the native variable descriptor; a separate host unit test
  checks the actual descriptor address and both pointer/flag indirections.
- The harness still omits native function providers and leaves error-return
  function stubs. This is not evidence that imports are usable at runtime,
  module startup succeeds, TLS works, or the title boots. No target instructions
  were executed and the Switch was not modified.

## Host TLS storage groundwork after a8927c4

- Main metadata declares TLS initialization at module-segment offset `0x878320`,
  with 56 initialized bytes and 128 total bytes. The initialization ends exactly
  at that segment's file-backed end, so the runtime's range validation accepts it.
- A byte inventory found no TPIDRURO-read candidates in main, FIOS2, or Ult, and
  one Thumb candidate in libc at original VA `0x810056ac`. SDK objdump showed the
  instruction inside TLS setup code, followed by a store; it is not a standalone
  read-and-return thunk. This is local static evidence, not execution coverage.
- The pinned libnx32 `__aeabi_read_tp` reads Horizon TPIDRURO, then loads its
  native thread pointer from offset `0x1f8`. Disassembly of the current runtime
  showed no reads/writes using the writable thread-pointer register. That is a
  candidate mechanism for future virtualization, not proof it is safe or preserved
  in every runtime path or context switch.
- Main TLS template snapshots and per-thread storage initialization are now
  implemented and host-tested, including independent initialized bytes, zero BSS,
  fixed-width kernel slots, allocation failure, and image cleanup retries.
  Guest register retargeting and dynamic module TLS are not implemented. No target
  code was executed, and the Switch was not modified.

## Host thread-pointer plan integration after 110de44

- Added optional source-bound `.tlsplan` sidecars and an explicit-site generator.
  No plans are discovered, generated, installed, or deployed automatically.
  Plans match the original file's size/FNV-1a fingerprint and original instruction
  words. FNV-1a is an accidental-change check, not authentication.
- A local plan for the previously inspected libc Thumb site at segment 0,
  offset `0x56ac`, matched plaintext SELF SHA-256
  `2c089f1ffb1ed62a42a2eaace23a55b126bd9efa29b02a0908ba67aede0a5ae8`.
  The host replay changed word `0x0f70ee1d` to `0x0f50ee1d`; a mismatched
  source fingerprint failed without writes. All four images still completed
  mechanical binding afterward, with the same native-function-stub limitations.
- The opt-in process path exchanges TPIDRURW for each thread's user TLS pointer
  before guest entry and restores its previous value on normal return. Host
  mocks verify opt-in behavior and restoration. ARM disassembly verifies that
  the exchange uses MRC/MCR opcode 2 and does not write Horizon TPIDRURO.
- This has not executed on Switch hardware. Context-switch preservation,
  native callbacks, module initialization, dynamic TLS, and title boot remain
  unverified. The local plan and private target binaries were not deployed.

## Owned TLS probe build after 88ed7d9

- Added a freestanding owned guest with named ARM and Thumb thread-pointer read
  helpers, plus a source-bound plan generated from their verified symbol locations.
  Ten imports were audited against runtime source providers.
- The pinned converter leaves TLS fields zero, so the builder explicitly writes
  this owned guest's descriptor for a 4-byte seed and 16-byte TLS area. Final-image
  validation confirms the seed bytes, descriptor, imports, and both patch words.
- The guest checks pointer ownership before any user-TLS dereference, performs
  32 native yields per thread, joins a normally returning worker, and verifies
  distinct TLS pointers and private markers. A failed/unverified join records a
  failure and parks rather than requesting cleanup with a potentially live worker.
- The actual C probe source passes an ASan/UBSan host model for the normal path,
  invalid thread-pointer value, worker initialization failure, and persistent or
  intermittent report-write failure. ARM hardware behavior remains untested.
- Outputs are staged locally only. No ARM guest instructions were executed and
  no files were deployed to the Switch.

## Host process-parameter settings after 1951596

- Extended bounded process-parameter decoding to indirect main-priority,
  main-stack-size, and preload-disable pointers. Unreadable or misaligned pointers
  fail decoding without publishing partial settings; missing fields retain defaults.
- In the supplied main executable, replay decoded requested Vita priority 64 and
  stack size `0x40000` (256 KiB). The preload-disable pointer was absent. These are
  static parameter values, not evidence that the main thread ran.
- Main-thread creation now uses the decoded priority/stack; the existing Vita-to-
  Horizon priority mapping remains in use. Host tests check nondefault settings,
  default-priority flags, invalid priority, and zero/overflowing stack sizes.
- Four-image host binding still completes with the existing native-function-stub
  limitations. Automatic preload selection, names, attributes, and affinity remain
  unfinished. No ARM guest execution or Switch changes were performed.

## Host app-bundled preload selection after 53aee8c

- Supported modern process metadata now selects existing app-bundled libc and
  FIOS2, in that order, while respecting their separate preload-disable bits.
  An explicit modules.txt, even empty, overrides automatic selection. Legacy or
  missing metadata retains single-image loading.
- Discovery is limited to those two filenames. No firmware search, Ult preload,
  dynamic sysmodule support, or execution compatibility is implied. Present
  malformed modules fail preparation rather than silently selecting HLE.
- Host tests exercise ordered discovery, missing files, each disable bit,
  legacy metadata, explicit overrides, discovery failure, and preparation
  rollback. No Switch files were changed and no commercial code was executed.

## Host shader-program queries after ca4dea1

- Added GXP magic checks, shader type/size, parameter count/index queries, and
  semantic lookup including the internal alias. Name lookup now bounds relative
  record/string ranges to the declared program size. Callers still must supply
  readable buffers matching that size; this is not a guest-memory sandbox.
- Patched vertex/fragment objects retain their caller-owned source GXP pointer
  so their GetProgram queries do not depend on the registration ID staying live.
  The application must keep its source program readable for the object lifetime.
- Host tests use the pinned repository's real vertex/fragment GXP fixtures and
  reject invalid table offsets/counts, signed name offsets outside the program,
  and unterminated names. No new rendering result or game boot is established.

## Host texture descriptor setup after 118acce

- Added linear-strided initialization with four-byte stride alignment and the
  pinned descriptor codec. Its 13 exposed stride bits represent 4..32768 bytes;
  larger otherwise API-valid strides return unsupported instead of truncating.
  Invalid dimensions/alignment leave the previous descriptor unchanged.
- Added public/internal width and height setters for the supported 2D layouts.
  Swizzled dimensions require powers of two; strided dimensions preserve the
  shared sampler/stride fields. Cube setters remain unsupported.
- Tests check packed descriptor words and row copying through the actual layout
  and staging helpers, including padding removal/clearing and resized dimensions.
  This is host evidence; no new scene or commercial title was run on the Switch.

## Host positional file I/O after 3526c51

- Added SceLibKernel Pread/Pwrite with signed 64-bit offsets. The pinned native
  library lacks positional I/O symbols, so a per-handle lock serializes cursor
  save/seek/transfer/restore against normal Read/Write/Lseek. The handle-table
  lock also prevents concurrent Close from reclaiming an active operation.
- Tests cover partial reads/EOF, sparse offsets beyond 4 GiB, invalid inputs,
  read-only write errors, each seek failure phase, and restoration after transfer
  failure. A restore failure returns an error; successful I/O is not claimed in
  that case. Append-mode writes retain the native append behavior.
- Threaded checks verify locks at blocked native calls and interleave normal
  sequential reads with positioned reads/writes on the same handle. Async
  Pread/Pwrite remain unimplemented. No FIOS or game execution is established.

## Host lightweight conditions after a5a6f54

- Added six public SceLibKernel LwCond imports using the pinned VitaSDK
  signatures/NIDs. The work area is 16 bytes, with a UID in the first word;
  native queues and mutex ownership stay outside guest memory.
- Waiter insertion and release of one associated mutex count share the mutex
  backend's lock. Signals select one FIFO/priority waiter, all waiters, or a
  pending target UID. Signals without a pending waiter are not retained.
- Signaled waits reacquire one mutex count with the remaining original timeout.
  Error/timeout returns leave that count released. These count/timeout rules
  follow pinned Vita3K 1387f8558d909e8fe05138b1a4e3aba4fe2b0e95
  `kernel/src/sync_primitives.cpp`; hardware equivalence remains unverified.
- Condition/mutex deletion wakes affected waiters with distinct errors. Retained
  object references prevent reused work areas from changing old waits, including
  waiters already reacquiring their mutex. Host ASan/UBSan tests cover queue
  order, broadcast, targeted signals, timeout in either phase, recursive counts,
  deletion/reuse, and the existing mutex behavior. Callback waits, condition info,
  thread cancellation, priority inheritance and exact invalid-target error
  behavior remain unfinished. No hardware files were changed.

## Host memory-block lookup after d3a0bf8

- Added `sceKernelFindMemBlockByAddr` with size-aware containment in tracked
  guest allocations. A zero-length query still requires an interior address;
  padding beyond the requested allocation and one-past-end addresses do not
  match. The UID is copied while holding the table lock. Range validation follows
  the SDK's size parameter description; exact hardware edge errors are unverified.
- Existing base/mapping queries now copy their result before dropping the table
  lock. Free atomically looks up and removes its record under the write lock,
  preventing simultaneous frees from reclaiming the same allocation twice.
- ASan/UBSan tests force reclamation between a query's unlock and return, check
  concurrent double-free ownership, and cover range boundaries and overflow.
  Returned guest pointers and GPU mapping handles remain borrowed: applications
  must keep their allocations alive while using them, including queued GPU work.
  No new commercial code or hardware test was executed.

## Host module information after f11b3ae

- Added SceLibKernel GetModuleInfoByAddr and SceModulemgr GetModuleInfo/
  GetModuleIdByAddr. Runtime registration assigns unique IDs before dependency
  startup; queries return copied metadata for the original guest segment ranges.
  The main executable and resident dependencies are included. Dynamic load/unload,
  module-list enumeration, and module exit callbacks remain unfinished.
- The loader now retains bounded module name/version/attributes, loaded segment
  permissions and sizes, source path (bounded to its ABI field), accepted lifecycle
  entries, TLS source address/sizes, and ARM exception-table addresses. Module
  state tracks ready/start-success/main-return transitions using atomic values.
  ARM compilation asserts the 0x1B8 module and 0x18 segment structure sizes.
- Actual target metadata replay exposed segment-relative EXIDX/EXTAB offsets.
  Resolving those offsets through the loaded module-info segment passes for all
  four supplied images and the owned fixed/TLS/surface/draw/cube fixtures. The
  fixed main's TLS source is 0x81878320 (56 initialized,
  128 total bytes); its EXIDX range resolves to 0x81780868..0x817BCD50. These are
  static/replayed addresses, not evidence of commercial code execution or C++
  exception unwinding support.
- Host tests cover malformed ranges, RX/RW alias separation, TLS and exception
  metadata, bounded output sizes, IDs, live state queries during owned dependency
  startup, and registry teardown. A two-library test caught host compiler padding
  between native library descriptors; explicit record alignment now preserves
  the section's array stride. No Switch files were changed.

## Host thread events after cf2bc6d

- Added public/raw thread-event registration and unregistration for START (4)
  and EXIT (8), with specific-thread and process-all (0x10027) selectors. Runtime
  object IDs now skip 0x10000..0x100FF to avoid collisions with pseudo-handles.
- Ordinary Capstone disassembly of the private libc copy located a registration
  call using selector 0x10027, mask 8, and a Thumb handler. The selector/signature
  agree with current VitaSDK documentation. REA native providers were unavailable;
  this is static disassembly evidence, not REA decompilation or code execution.
- START runs after target TLS/experimental TP setup. EXIT runs once on normal
  return, explicit ExitThread/ExitDeleteThread, and the ExitProcess path, before
  thread retirement. Dispatch holds no registry lock while calling guest code.
- Retained records permit self/concurrent unregistration. Removed callbacks are
  skipped on future selection; already-selected callbacks may still run. A dispatch
  snapshot excludes callbacks registered during that dispatch. Native callback
  frames are released on a nonreturning exit, including recursive EXIT calls.
- ASan/UBSan tests cover selectors, mutations, two simultaneous callbacks during
  unregister, allocation failure, TP/TLS context, normal and callback-triggered
  exit, raw options, and process-exit wiring. Callback argument 3 is zero and
  nonzero returns are diagnostic, following pinned Vita3K. Exact ordering/error
  behavior and ARM execution remain unverified; forced worker shutdown, thread
  restart, and other callback classes remain unfinished. No Switch files changed.

## Host Clib bootstrap imports after aaf24f8

- Added SceLibKernel exports for Clib Memcmp, Strncmp, Strncpy, Strrchr,
  ASCII Strncasecmp, Snprintf, and Vsnprintf. Native memory/string interfaces
  share the ARM32 pointer/size ABI; formatting writes the caller's buffer and
  exposes no native FILE or allocator objects to guest libc.
- Formatting returns the would-be length, including zero for empty output,
  and maps negative backend results to INVALID_ARGUMENT. This avoids the pinned
  emulator's old zero-success/empty-error behavior, following upstream issue
  [4401](https://github.com/Vita3K/Vita3K/issues/4401) and the corrected result
  handling in [4441](https://github.com/Vita3K/Vita3K/pull/4441). Basic Snprintf
  length/truncation cases are also in the VitaAutotests
  [recorded format results](https://raw.githubusercontent.com/Vita3K/VitaAutotests/master/tests/clib/format.expected).
- ASan/UBSan tests cover unsigned byte comparison, padded string copies, ASCII
  folding, bounded/truncated/empty formatting, zero-capacity buffers, mixed
  integer/double/string arguments, va_list forwarding, and backend failure.
  Both installed compilers pass base-PCS/four-byte-va_list/scalar-size checks;
  their emitted objects specify eight-byte stack alignment. These are host and
  compile-side checks, not execution of guest ARM calls.
- Formatting uses native newlib. Firmware-specific exponent/general-float,
  unknown-conversion, and null-string quirks in the recorded tests are not
  reproduced. Full formatting parity, Clib mspace heaps, and checked variants
  remain unfinished. No Switch files were changed.

## Host process parameters after fe3ca2b

- Added SceProcessmgr `sceKernelGetProcessParam` for the loaded main image.
  It returns the original guest address, including legacy version-0 structures,
  or null when absent. Dependencies cannot replace the main image's parameters.
- The loader validates the entire declared structure before retaining its address.
  Runtime registration checks alignment and containment in a main PT_LOAD segment,
  uses executable RX/data guest addresses, and rejects hidden write aliases.
  Registration precedes guest startup; teardown clears the exposed address.
- Host ASan/UBSan tests cover NID lookup, main ownership, duplicate registration,
  absent and legacy parameters, malformed ranges, alias rejection and teardown.
  Both ARM Debug and Release builds pass.
- A private four-image loader replay recovered the main process parameter at
  `0x8187a3b4`, size 48, with libc parameters at `0x817c5cdc`. This replay performs
  relocation and metadata parsing on host buffers; it does not execute guest ARM
  code or establish bundled-libc startup or commercial-title compatibility.
  No Switch files were changed.

## Host dependency-start threads after aead0bb

- Static inspection of the privately supplied libc identified import stub
  `0x8102d480` as SceThreadmgrCoredumpTime `sceKernelExitThread` (NID
  `0x0c8a38e1`). Its module-start success path calls that stub with status 0;
  early failure paths call it with status 2. The earlier inline startup design
  would therefore terminate the guest main thread before entering the game.
  This is import-table/disassembly evidence, not guest execution evidence.
- Dependency starts now run sequentially on fresh guest threads, inheriting
  their caller's priority and using a 256 KiB stack. Each gets the normal TLS,
  optional reviewed thread-pointer exchange, and START/EXIT event path. The
  parent joins and deletes each startup thread before advancing. Return and
  explicit ExitThread status both feed the existing resident-success gate.
- A host join error is retried while retaining the process, rather than allowing
  teardown of mappings still used by an unconfirmed live startup thread.
  ExitProcess from a dependency also ends the parent after joining the child,
  preserving the process result and preventing entry into main.
- A pthread-backed integration test uses the production loader/thread wrappers
  and real callback registry. It checks explicit exits, normal returns, failure
  gating, inherited priority, isolated TLS contents, event delivery, cleanup,
  create/start failures, a transient join failure, and process-exit propagation.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- Per-module custom thread settings, stop/exit lifecycle callbacks, dynamic
  module loading and shutdown of unrelated guest workers remain unfinished.
  Bundled-libc and commercial-title boot still require hardware validation.
  No Switch files were changed.

## Host standard streams after b7097a1

- Added SceProcessmgr GetStdin/GetStdout/GetStderr NIDs, publishing a fresh guest
  file UID per call in the existing I/O registry. Handles borrow native standard
  descriptors; guest close removes only its wrapper and cannot close native
  stdin/stdout/stderr or invalidate another guest wrapper.
- Reads and writes use native byte-oriented I/O, retaining short transfers,
  errors, binary data and EOF. Guest stdin is read-only; output/error handles
  are write-only. Terminal handles reject normal and positional seeks with
  ESPIPE. Allocation failures publish no descriptor.
- Host tests exercise actual pipe I/O through NID-resolved getters, embedded NUL
  and percent bytes, EOF, mode and pointer errors, multiple output wrappers,
  protected native descriptor ownership, concurrent getters/writers/close, and
  allocation failures. Existing file-handle lifetime tests remain applicable.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- VitaSDK declares these as file UIDs; current Vita3K opens a separate tty handle
  for each getter. Fresh-handle policy follows that implementation, not a new
  hardware observation. References:
  https://docs.vitasdk.org/kernel_2processmgr_8h.html and
  https://github.com/Vita3K/Vita3K/blob/master/vita3k/modules/SceProcessmgr/SceProcessmgr.cpp
- Native standard descriptors must remain stable for the lifetime of borrowed
  guest wrappers. Interactive terminal input, firmware-exact error precedence,
  and terminal-specific ioctl/stat semantics remain unverified. Output is routed
  to the native runtime's configured console, which may be silent; this does not
  add SD/network logging or establish bundled-libc startup or commercial boot.
  No Switch files were changed.

## Host libc time imports after 866f74d

- Added SceProcessmgr LibcClock, LibcTime and LibcGettimeofday exports. LibcClock
  uses the existing process-relative monotonic microsecond counter and returns
  the SDK's full 64-bit SceKernelClock, including values above UINT32_MAX.
- Calendar calls read native UTC gettimeofday into a native temporary, then
  convert fields explicitly. SceKernelTime is 32-bit; guest timeval is two
  signed 32-bit fields. Epoch seconds retain their low 32-bit representation,
  including post-2038 bit patterns. No host time_t, long or timeval layout is
  copied into guest memory. Static ABI assertions cover all four SDK types.
- Gettimeofday permits optional outputs and reports UTC (zero timezone value).
  With no timeval requested it needs no native clock read. Native failure returns
  -1, and Time returns its unsigned -1 sentinel; output objects stay unchanged.
  Guest errno propagation and firmware-exact failure/rollover behavior are not
  established by these choices. Non-UTC timezone/DST support remains unfinished.
- Host tests query the actual NID tables and cover clock high bits and restart,
  microsecond endpoints, guarded output sizes, 1970/current/2038/2106 boundaries,
  negative epochs, optional outputs, UTC initialization and backend failures.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- ABI reference: the pinned vita-headers checkout and
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2common/kernel/processmgr.h
  distinguish the 64-bit clock from 32-bit epoch seconds. No new Switch run,
  bundled-libc startup or commercial-title boot is claimed by these host checks.

## Host file metadata changes after ff194f2

- Added SceLibKernel sceIoChstat (NID 29482f7f): selected permissions, size,
  access time and modification time use native chmod/truncate/utimes. Paths use
  the existing app0 resolver and native failures propagate as Vita errno values.
  The initial ENOSYS mapping was 0x8001004e; the following path-operation
  change corrects it to the VitaSDK libc value 0x80010058.
- Validate masks, negative sizes and selected calendar fields before mutation.
  Creation-time changes return ENOSYS before any other field changes: POSIX ctime
  is not a settable creation date. Unknown mask bits return EINVAL. Type bits do
  not change file type; group permission bits are ignored per the SDK fields.
- UTC Gregorian conversion avoids host timezone/DST normalization, validates
  leap years and microseconds, and rejects dates outside years 1..9999 or native
  time_t range. Unselected timestamps use a pre-change snapshot rounded to
  microseconds. Native operations are sequential, so a later failure may retain
  earlier successful changes; this is not an atomic metadata transaction.
- Getstat and descriptor/directory stat now reflect native owner/system permission
  bits and subsecond timestamps instead of fabricated writable modes and zero
  microseconds. Vita system permissions map to POSIX other bits in this bridge;
  firmware enforcement and SD permission/precision behavior remain unverified.
- The host regression uses only temporary files, including a sparse size above
  4 GiB, permission/type readback, exact microsecond round trips, unchanged
  unselected timestamps, invalid combined requests, UTC conversion across
  centuries under a non-UTC host timezone, and forced backend failures.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- ABI reference:
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2common/kernel/iofilemgr.h
  No Switch files or original game files were changed. This does not establish
  bundled-module startup or commercial-title boot.

## Host filesystem path operations after 1a09342

- Added SceLibKernel Mkdir (9670d39f), Rmdir (e9f91ec8), Remove (e20ed0f3)
  and Rename (f737e369), all imported by the private bundled FIOS module.
  Paths use the existing resolver; mkdir does not create missing parents,
  file removal uses unlink, and rmdir is nonrecursive.
- Both rename paths are resolved before the native operation. Replacement and
  open-file semantics are left to native rename, without an unlink/copy fallback
  or forced guest descriptor closure. Host tests verify replacement contents,
  continued access through old open file descriptors, and source preservation
  when destination validation or the native rename fails.
- Corrected the previous ENOSYS mapping from the SceNet value 0x4e to SDK libc
  88 (0x58). Added explicit EPERM, EBUSY, EXDEV, EROFS, ENOTEMPTY (90/0x5a)
  and ELOOP (92/0x5c) mappings; Linux errno values must not leak into the Vita
  low word. The pinned VitaSDK arm-vita-eabi sys/errno.h confirms these values.
  https://github.com/vitasdk/newlib/blob/master/newlib/libc/include/sys/errno.h
- Regression tests use temporary directories only and query actual NID exports.
  They cover permissions, existing/missing entries, file/directory type errors,
  empty/null/unsupported-device paths, lexical app0 escape rejection, oversized
  rename destinations, open-file lifetime, and injected cross-device/read-only/
  busy/unimplemented errors. The metadata test now checks the corrected ENOSYS.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- Firmware-exact replacement/error precedence, SD behavior, asynchronous variants,
  new writable mount policies and savedata routing remain unverified/unfinished.
  Directory-entry metadata still uses stored paths: renaming an open directory
  is not established by the open-file tests. No Switch or original game files
  were changed, and commercial-title boot is not established.

## Host FIOS process/thread queries after 180670e

- Bounded static Capstone disassembly of the private FIOS ELF (not execution,
  and not an REA provider result) followed module_start at original guest VA
  0x81000001 into initialization at 0x81001b74. It creates lightweight mutexes
  and conditions, then calls import stub 0x8101ad04 at 0x81001c08. The import
  table identifies SceThreadmgr sceKernelGetProcessId (9dcb4b7a). Initialization
  then enters an overlay-list wrapper; its packed syscall argument layout still
  needs validation before implementing that query. REA native tools/providers
  remain unavailable in this chat; no registration/setup retry was performed.
- Processmgr init now allocates a guest process UID from the shared Sysmem UID
  namespace. The atomic GetProcessId query returns the same UID for every thread
  until finish, which clears it. A later successful process init obtains a new
  UID; duplicate/invalid init cannot replace an active one. UID allocation failure
  publishes neither identity nor parameter address. Host queries outside the
  registered process return UNKNOWN_UID; that is not a hardware-observed case.
- The FIOS import table also contains SceThreadmgr library 859a24b1, NID 01414f0b.
  The SDK kernel table names that NID GetThreadCurrentPriority. Added its direct
  import alias to the existing current-guest-thread implementation, returning
  the resolved Vita priority rather than Horizon's mapped scheduler priority.
- Host tests resolve both NIDs, check stable process identity across concurrent
  readers, fresh identity after reinit, failure preservation and teardown, and
  raw/public priority agreement with and without a current guest thread.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
  No new Switch run or bundled-FIOS/commercial-title boot is established.

## Host initial FIOS overlay discovery after 4b31fe9

- Static Capstone inspection of the private FIOS wrapper at original VA
  0x8101a7c4 showed a 24-byte syscall block. Its first words contain ID-buffer
  address (+0), requested ID count (+4), returned-count address (+8), and
  copy capacity (+12), calculated as min(requested IDs, 128) * 4. The final
  eight bytes are unspecified stack contents, not validated options.
- Added SceFios2Kernel GetList (9379e2d5) with an explicit 32-bit packet prefix,
  current-process/self selection, order range, pointer alignment and copy-size
  checks. It reports zero overlays because this runtime installs none. This
  handles the observed initializer query; it does not install, enumerate a
  nonempty registry, modify, remove, or resolve overlays. Those imports remain
  unsupported, and this is not full FIOS overlay support.
- Added GetRecommendedScheduler (b02e0b26): a required non-NULL options pointer,
  index 1 for host[0-9]: when more than one scheduler is available, otherwise 0.
  It neither runs a scheduler nor adds a host-device mount. The options contents
  are unused. Rule reference:
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/fios2kernel.h
- Host tests map buffers into 32-bit address space, resolve real NIDs, and cover
  the observed zero-capacity query, packet offsets, 128-ID copy cap, ignored
  reserved fields, unchanged ID buffers, four-byte count writes, current/self
  process selection, malformed input without output changes, input/output aliasing,
  and scheduler index/prefix boundaries. ARM-only layout assertions compare the
  raw packet offsets with the pinned SDK's 24-byte syscall structure.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- The required count-pointer and validation/error choices beyond the observed
  initializer packet are not firmware-verified. HLE pointer inputs must refer
  to readable/writable mapped guest memory, as with existing native wrappers.
  No Switch files were changed; FIOS startup and commercial 3D boot remain
  unverified by this static analysis and host testing.

## Host disabled color surfaces after f263b31

- The source-export audit found shader-parameter queries already available; the
  private main still imported missing color-surface queries and disabled init.
  Added ColorSurfaceInitDisabled (613639fa), IsEnabled (0e0ebb57), GetData
  (2db6026c), and GetStrideInPixels (f33d9980).
- Moved the runtime-owned opaque color descriptor into surface.h, replacing its
  native pointer field with an explicit 32-bit guest address. Its ARM32 layout
  remains 48 bytes, data at byte 16 and background texture at byte 32; host
  static assertions now check the same layout. Descriptor copies retain all
  metadata and require no address-keyed side registry.
- Disabled init clears dimensions/data and initializes the pinned reference's
  null 1x1 BGRA linear background texture. Queries return actual descriptor
  fields. Normal init is now in the tested surface unit, validates pointer,
  dimensions, even stride, data alignment and swizzled power-of-two dimensions
  before changing output. It retains the previous normal background-texture
  behavior; general color-format/background texture population is unfinished.
- Scene load, native image view, scene export/discard and effective color-write
  masking all use enabled state. A non-NULL disabled color surface therefore
  suppresses color loads/writes/export while retaining depth/stencil processing.
  Fragment-program selection remains independent of color attachment presence.
- Host tests resolve the added NIDs, exercise enabled/disabled metadata and
  copied descriptors, check exact size/offsets and output guards, validate
  background texture fields, and preserve an initialized descriptor on invalid
  requests. The helper rejects disabled descriptors even with a nonzero stored
  data address. The full ASan/UBSan suite and ARM Debug/Release builds passed.
  Scene GPU behavior has not been executed on hardware here.
- Layout/initialization reference: pinned external/Vita3K SceGxm.cpp around
  ColorSurfaceInit/InitDisabled and the VitaSDK public ABI:
  https://docs.vitasdk.org/group__SceGxmUser.html
  Color-surface clipping, gamma conversion and broader render-target formats
  remain unfinished. No Switch files were changed; commercial 3D boot remains
  unverified.

## Host depth-transfer CPU groundwork after 262cd89

- Investigation of force load/store found the old native image helper directly
  treats guest depth storage as a native image. deko3d explicitly forbids pitched
  depth images; Vita tiled layouts also need conversion. Do not expose setters
  that activate that path before replacing it with transfer staging.
  https://github.com/devkitPro/deko3d/blob/master/source/dk_image.cpp
- Added a CPU transfer plan and read/write codec for linear DF32, S8 and separate
  DF32_S8 planes. The staging sample stores depth as an unchanged 32-bit float
  bit pattern and stencil in the low byte of a separate word. This is a CPU
  representation, not a verified declaration of NVIDIA packed image storage.
- Plans check dimensions, the guest's 32-pixel stride units, independent sample
  row stride, and ARM32 span limits. Required spans include only active pixels
  in the final row. Codecs revalidate copied plan sizes and all buffers before
  output, reject pointer wrap/overlap (including plan metadata), preserve padding,
  and use supplied backgrounds for explicitly unbacked planes.
- Regression checks exact signed-zero, signaling/quiet NaN payload and infinity
  bits, separate stencil round trips and low-byte truncation, padded rows,
  absent planes, unsupported tiled/other formats, malformed sizes/plans and
  failure preservation. A local Bonsai test draft was rejected for incorrect
  units/SDK stubs; the checked-in tests use the actual SDK types.
  The full ASan/UBSan suite and ARM Debug/Release builds passed.
- GPU allocation/upload/readback, native image storage conversion, format/layout
  expansion and scene load/store integration remain required. Force load/store
  exports are deliberately not added by this groundwork commit. No new runtime
  depth-buffer transfer, Switch test, or commercial boot is claimed.

## Runtime 15d400f48e5d3a3f5d811e2a67bd30a1388cc102

- Standalone HOME title, owned fixed-address CPU probe SHA-256
  `ea59fcf2675f9a786613048e11a646875379b40d703c73564358171fd210bdc5`.
- User reported that the first launch took a while and requested another layout;
  after a manual relaunch it returned to HOME without an error. The earlier
  exhausted/failing sequence is not preserved in the two recovered log files.
- The recovered previous log shows code range `0x81000000` conflicting with
  alias reservation `0x49800000` + `0x40000000`, followed by automatic restart
  attempt 2/64 under the expected program ID `0x0100f27432680000`.
- The final log identifies `vita2hos_fixed_probe`, RX code at `0x81000000`,
  RW data at `0x81879000`, and reports a compatible layout after 2 restarts.
  Guest exit status and process result were both 0; image cleanup returned 0.
  The probe checks its actual PC, initialized data, zero BSS, and a data write.
- This verifies native fixed-address execution and retry-count forwarding on
  this run. It does not establish the success rate, the 64-attempt boundary on
  hardware, TLS initialization, bundled-module startup, or commercial game boot.

## Runtime f55edbf626acb6ff46d510a55bdd026dd78ac5ad

- Experimental route: a retargeted ARM32 ExeFS NSP at
  `atmosphere/contents/0100152000022000/exefs.nsp` for Mario Kart 8 Deluxe.
- User-reported outcome: normal launch displayed **"Could not start the software"**.
  Holding R still opened normal hbmenu.
- No `surface-probe.txt` or runtime log was found through USB/MTP after this
  attempt. There is no evidence that the runtime or guest executable started.
- Rollback: the installed file was checksum-verified and renamed to
  `exefs.nsp.disabled-f55edbf-20261127-035116`; a subsequent directory listing
  confirmed no active `exefs.nsp` remained. Global hbmenu configuration was not
  modified. Normal game launch after rollback still needs user confirmation.
- Treat this title-replacement route as failed on this setup; it is not a
  validated deployment method. Investigate a standalone installable title.
- The standalone title `0100f27432680000` installed successfully, but the user
  reported **"Could not start the software"** with no error code on launch.
  No runtime/probe report was found afterward. Installation is therefore
  demonstrated; runtime execution is not.
- Source inspection found `allow_debug=true` and `force_debug=true` in the NPDM
  configuration. Atmosphere's current `CapabilityDebugFlags::IsValid` rejects
  multiple enabled debug flags; its hbloader path explicitly rewrites them.
  This is a concrete metadata defect and a candidate explanation, not proof
  that no other startup problem exists.

## Runtime 221600951699dcad7f0592fab00efac8a30132a8

- Delivery: Debug NSP from GitHub Actions run 45900467990, packaged with the
  upstream alpha-0.4 cube samples and uploaded through FTP.
- Configuration: Atmosphere application override with `32_bit` address space.
- Untextured cube: user reported **"cube works"** after deployment.
- Evidence type: user-reported hardware result; no screenshot, video, or log
  was supplied. Continued animation, depth correctness, duration and performance
  were not separately reported.
- Textured cube: pending.
- Commercial 3D titles, including LittleBigPlanet: not tested.

This is evidence of a basic 3D boot on the user's Switch, not proof of broader
game compatibility. Keep the runtime fixed when first switching to the textured
cube so the test changes one component at a time.
