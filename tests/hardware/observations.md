## 2026-11-27: owned kernel probe on runtime 3dda5cf - PASS

User ran kernel_probe v1 and reconnected USB/MTP. The active executable matches SHA256 71f3ec8dca3d9bbc823f94ce499e9c04c97652603bb5bc8bc189aade2f890879 (17243 bytes). The collected report passes retained/duplicate/timeout signals, mspace pool bounds and exhaustion, 128 signal/mspace worker roundtrips, and named-condition mutex release/reacquire. Runtime log identifies 0.4.0-3dda5cf, contains no unresolved imports, and reports guest/process/launch/close zero.

This verifies the owned kernel paths exercised by this probe. It does not verify RWlocks, affinity, power tick, missing-import tracing, commercial library startup, or LittleBigPlanet boot. The startup-capable runtime is now confirmed installed; the active payload remains the owned kernel probe. Raw report and log were collected privately in result-20261127-140649.

## 2026-11-27: ABGR8 surface gamma on runtime 4d1233b — PASS

User installed HOME title 4d1233b and ran visible_cube v3. Active guest SHA256 9f61826b2e5bcfebb0ff2788ef97fa5adaddf8090c0e6593d9b0144c74eb9d40 matches the transferred probe. Collected report passes color_surface_gamma_rgb_alpha_and_reset: nine interior pixel samples per pass check mid-gray RGB near 188 with linear alpha near 128 under BGR gamma, then RGB/alpha near 128 after switching to NONE on the same target (tolerance two). The report also passes driver memory, framebuffer switching, null color, shader red, depth-only occlusion, stencil replacement, texture changes/viewports and all 360 animated cube frames. Runtime log identifies 4d1233b, has no unresolved imports, and guest/process/launch/close return zero.

This verifies the supported linear ABGR8 output gamma path and reset. It does not specifically verify destination blending with sRGB, selective GR gamma, other surface layouts/formats, commercial game execution, or commits after 4d1233b. No later runtime has been transferred.

## 2026-11-27: owned dependency probe on runtime 006d5c4 — PASS

User ran module_probe v1 from source c3ee39d on the installed HOME title. Collected report passes main_tls, dependency_exit_thread_start, function_import, variable_import_alias, and final module_probe. Log identifies runtime 006d5c4, applies two reviewed TP sites, starts owned_dependency in a separate module_start thread, and returns guest/process/launch/close zero without unresolved imports. Active executable SHA256 05ab4d2942cb13791645696cf25736a8653944390772e14eb5fe2a85db595283 and TLS plan 461e99661fc7e840a09b91bdf91341a118539e7b0e9a4ff881ae7603d06f3b60 match the transferred payload.

This proves the owned dependency path, including successful startup through sceKernelExitThread and imported writable state. It does not prove commercial libc/FIOS/Ult startup, module-specific TLS, or a commercial title boot.

## Owned dependency-start probe (local preparation)

- Added tests/hardware/module_probe with an owned main executable and an owned
  dependency selected by modules.txt. The dependency exports one function and
  one variable, records its kernel TLS address/start count, then ExitThread(0).
  Main verifies separate startup-thread storage, main TLS, function binding and
  shared writable storage via the imported variable before reporting PASS.
- The builder compiles both ARM images with warnings as errors, generates the
  stub library with VitaSDK, audits native imports, validates matching library/
  symbol tables and export target segments, and binds two reviewed ARM/Thumb TP
  instructions to the final main executable fingerprint. Dependency module TLS
  is zero; this does not test module-specific or dynamic TLS.
- A private host-only replay of the actual loader on these owned payloads passed
  publication/binding and patched main's variable reference at offset 0x248 to
  the dependency's simulated writable address. Main TLS metadata was 4/16 bytes.
  No ARM code ran in that replay; native HLE imports were not executed.
- Planned hardware runtime is already installed 006d5c4. No newer mutex runtime
  or commercial module is required. This is preparation, not a hardware result.

## Offline named mutexes (after 006d5c4 hardware baseline)

- Added SceLibKernel CreateMutex (0xED53334A) and LockMutex (0x1D8D7945),
  SceThreadmgr raw create/lock (0x92667AE5/0x7FA945AD), TryLockMutex
  (0x72FC1F54), UnlockMutex (0x1A372EC8), and DeleteMutex (0xCB78710D).
  These use real UID-selected ownership/wait queues. Named UIDs cannot select
  lightweight mutex objects.
- Named objects retain an internal work area and reuse the lightweight mutex
  queue machinery under the same lock. FIFO and priority-at-enqueue policies,
  recursive counts, overflow/underflow, owner checks and microsecond timeout
  accounting are supported. Errors use the named mutex group.
- Deletion unlinks the object, wakes blocked callers with WAIT_DELETE and retains
  storage until waiter/condition references retire. Shared unlock/delete helpers
  preserve existing lightweight behavior. Installed runtime 006d5c4 is unchanged.
- Host tests cover NIDs, option/UID failures, recursion/overflow, wrong-owner
  unlock, try contention, FIFO/priority order, timeouts with a remaining waiter,
  deletion with a blocked waiter, 128 try/delete races and cross-kind handles.
  The full ASan/UBSan suite, including lightweight mutex/condition regressions,
  final focused tests and ARM Debug/Release builds passed.
- A bounded Bonsai review suggested timeout-removal and delete-race cases.
  Automatic owner-death cleanup, priority inheritance/ceiling, named open/close,
  callbacks and named condition/RW-lock APIs remain unimplemented. Unsupported
  attributes/nonzero ceiling options are rejected. This new path has not been
  hardware-tested, and commercial game boot remains unverified.

## Runtime 006d5c495f2265d877cc14a94ab63b8843136cbf — owned TLS v1

- User completed the probe; current report/log were collected via USB/MTP.
  Guest SHA-256 277f0b0bb34f5a342582f07582631bba2842bca0202f0f23b27e2ef706082aba,
  plan SHA-256 5441186979d3e98c77e3758b5b2942d8b6db162c7058a6a522d65588448e55ea.
- Report: PASS main_tls, PASS worker_tls, PASS thread_isolation, PASS tls_probe.
  The owned test checks ARM/Thumb thread-pointer reads, initialized TLS/BSS,
  native calls/yields, separate worker TLS and main pointer after worker join.
- Runtime log identifies 006d5c4, applies exactly two reviewed owned patch sites,
  and reports guest/process/launch exit 0 and load_process_close 0. No unresolved
  imports were observed. The earlier cube report/log remain preserved.
- This verifies the opt-in source-bound TPIDRURW path for this owned workload on
  the physical Switch. It does not validate commercial libc/other module starts,
  unreviewed TLS instructions, dynamic TLS or LittleBigPlanet boot.

## Runtime 006d5c495f2265d877cc14a94ab63b8843136cbf â€” owned cube v2

- User completed the test; report and runtime logs were collected over USB/MTP.
  Executable SHA-256: 6ed20aab2a8cae81531906923b3d0c92dc2fc31720dcc56cb889f3c69d74652c.
- Report starts visible_cube v2 and ends PASS visible_cube after all 360 frames.
  Driver-memory ownership/lookup, framebuffer switching, null color scene,
  background depth, shader color, depth-only occlusion, stencil replacement,
  texture quadrants/updates and all listed viewport orientations passed.
- Current log identifies runtime 0.4.0-006d5c4, process result 0, launch result 0,
  and load_process_close result 0. No unresolved-import or deko3d failure was
  observed in the recovered run. The previous log was preserved separately.
- This verifies this owned 3D workload on the user's physical Switch. It does
  not separately exercise forced depth load/store, sRGB gamma, alternate polygon
  modes, commercial shaders, library starts, or TLS register preservation.
  LittleBigPlanet boot remains unverified.

## Offline render-target driver memory (2026-11-27)

- Added sceGxmRenderTargetGetDriverMemBlock (0x49553737). Render targets now
  reside in a real Sysmem allocation: driverMemBlock=-1 requests an owned
  uncached allocation; an existing positive UID supplies borrowed storage.
  GetRenderTargetMemSize returns this runtime's page-rounded metadata size,
  allowing direct allocation through Sysmem. This is not Vita driver-data layout.
- Creation validates dimensions/scenes/output pointers, snapshots params before
  touching supplied storage, checks block identity/base/size, and publishes the
  target only after both native shadow images are allocated. Owned driver memory
  and partially allocated shadow images are released after allocation failure.
  Supplied allocations must remain alive until target destruction; memory-type
  enforcement and arbitrary concurrent guest frees are not implemented.
- Native surface creation checks image size/alignment and GPU allocation before
  initializing images. Destruction is empty-safe and clears the descriptor.
  Render-target destruction flushes/waits for submitted render work before
  destroying shadows and freeing only owned driver storage. Callers must end
  scenes before destruction; queue waits cannot retire unsubmitted commands.
- Driver helper tests use real Sysmem code with a mock GPU mapper, covering
  owned/borrowed UIDs, page sizing, small/invalid blocks, mapping failures,
  failure preservation, and an ownership record embedded in its own allocation.
  Surface model tests cover failed allocation before image initialization,
  overflow, invalid alignment, image/page alignment and repeated destruction.
- The full ASan/UBSan host suite passed; final helper adjustments also passed
  focused checks. Final ARM Debug/Release builds passed after the helper
  adjustments.
- Surface/draw/cube probes now use driverMemBlock=-1 explicitly (versions 2/6/2).
  They check owned driver lookup, page sizing, a supplied UID and its survival
  after target destruction. VitaSDK builds and the cube's 60-import source audit
  passed. Old zero-initialized probes need rebuilding for this stricter API path.
- GPU shadow-memory sizes/formats are runtime-specific. Existing multisample and
  custom-location limitations are unchanged; no broader MSAA support is claimed.
  No new runtime or probe has been transferred to the user's Switch yet.

## Offline polygon rasterization state (2026-11-27)

- Added sceGxmSetFrontPolygonMode (0xFD93209D, imported by LittleBigPlanet)
  and sceGxmSetBackPolygonMode (0xF66EC6FE). The seven SDK modes map to native
  Fill/Line/Point using the pinned Vita3K renderer's mapping. Unknown values
  preserve state and log an error instead of silently selecting fill.
- Native context defaults remain fill. Both setters mark rasterizer state dirty;
  BeginScene and pre-draw replay bind effective state. When two-sided mode is
  disabled, the front polygon mode applies to both native faces. The stored back
  choice survives and becomes effective again after enabling two-sided mode.
- Effective-state construction copies the rasterizer descriptor and changes only
  polygonModeBack when required. Culling, winding, depth-bias enable flags and
  other rasterizer settings are preserved; context state is not overwritten by
  the temporary one-sided choice. Two-sided toggles now invalidate rasterizer
  state alongside existing stencil/depth/fragment state.
- Host checks cover mode translation, invalid/null outputs, distinct faces,
  temporary one-sided mode, preservation of unrelated rasterizer flags and
  independent context state. The deko stub bitfields match the installed SDK.
  The full ASan/UBSan host suite and ARM Debug/Release builds passed.
- Point modes follow native point rasterization; dedicated point-sprite UV
  generation and exact Vita point/line edge rules remain unimplemented or
  unverified. Native mapping is not evidence of rendered parity. No GPU test,
  Switch transfer, or commercial-title boot was performed for this change.
- A bounded Bonsai mapping draft was requested; it overran its read scope and
  compacted its context. The final function and integration were reviewed against
  the actual enum definitions and pinned renderer source.

## Offline VFP exception-mask state (2026-11-27)

- Added SceThreadmgr sceKernelChangeThreadVfpException (0xCC18FBAE), imported
  by the supplied FIOS library, and SceLibKernel sceKernelGetCurrentThreadVfpException
  (0xA37A6057). They operate on the calling guest thread's kernel TLS word 4.
- Only SCE_KERNEL_VFP_EXCP_ALL (0x0800009f) bits are accepted. Overlapping
  clear/set arguments and reserved bits return INVALID_ARGUMENT without writes;
  no current guest thread returns UNKNOWN_THREAD_ID. Newly created threads have
  their own zeroed mask; changing one does not alter another's mask.
- The setter follows the pinned Vita3K update and returns its previous mask.
  VitaSDK's prose instead says SCE_OK on success: nonzero return behavior remains
  a firmware-validation item. Sources:
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/kernel/threadmgr/vfp.h
  and pinned external/Vita3K/vita3k/modules/SceKernelThreadMgr/SceThreadmgr.cpp.
- This is kernel mask-state groundwork. It does not implement Vita exception
  delivery/handlers or claim native floating-point trap support on Horizon.
  The API mask bits must not be blindly written into FPSCR's cumulative status
  positions. Native FPSCR, arithmetic modes and sticky flags are unchanged.
- Focused ASan/UBSan tests check NID lookup, flag updates, previous values,
  invalid/overlapping masks, failure preservation, per-thread isolation and
  absence of a guest caller. ARM Debug and Release builds passed. No Switch
  deployment or hardware test occurred.

## Offline FIOS empty-overlay path resolution (2026-11-27)

- Added _sceFiosKernelOverlayResolveSync (0xE9AE60FB) and
  _sceFiosKernelOverlayResolveWithRangeSync (0x8CCA471A). With no installed Vita
  overlays, both return the input Vita path unchanged; host/device mapping stays
  in the I/O layer. Foreign process IDs, invalid write/range selectors, null
  pointers, unterminated inputs and insufficient copy capacity fail before writes.
- The initialized packet prefix is explicitly 32-bit: output address at +0,
  input/path capacity at +4, optional min/max order bytes at +8/+9, and independent
  output-copy capacity at +12. Uninitialized reserved/trailing words are ignored.
  Source SDK documentation also distinguishes validation and copy capacities:
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/fios2kernel.h
- Evidence: a private host-only replay applied the existing relocation code to
  the user-supplied FIOS ELF before bounded Capstone inspection. The resolve
  call at relocated 0x2801a7f6 follows stores of output at sp and capacities at sp+4/sp+12,
  and calls raw stub 0x2801a984. Range wrapper 0x2801a800 supplies the same
  fields at packet-relative offsets with min/max bytes +8/+9. No ARM guest code
  was executed. The unrelocated image did not yield reliable direct-call refs.
- REA tools/providers remain unavailable in this chat; this was the established
  static fallback, not a native decompiler result. Private code/dumps remain
  outside the repository. Thread-disable wrappers only pass the syscall result
  through; their setter return semantics remain unresolved, so those exports
  were not added based on guesses.
- Runtime policy currently caps max_path at 4096 bytes, requires a full string
  plus terminator to fit output capacity, and returns ILLEGAL_SIZE on failure.
  Exact firmware error/truncation behavior and disabled-thread handling remain
  unverified. Overlay registration and nonempty overlay rewriting are not added.
- Host tests check NID dispatch, independent capacities, non-NUL/oversized paths,
  range/PID/selector errors, unchanged output on failure, overlapping input/output,
  and output overwriting the copied syscall packet. The full ASan/UBSan host
  suite and ARM Debug/Release builds passed. Hardware startup is unverified;
  the Switch installation and active executable remain untouched.

## Offline RTC imports for bundled FIOS (2026-11-27)

- A fresh source/export-table comparison found 29 unresolved imports in the
  supplied FIOS module before this change (80 total, 51 source exports). This is
  availability evidence only: a resolved NID does not establish API semantics.
  Repeating the comparison after this change leaves 26 FIOS imports unresolved.
- Added SceRtcUser 0x6704D3C6 exports sceRtcGetCurrentClock (0x70FDE8F1),
  sceRtcSetTime64_t (0xA6C36B6A), and sceRtcGetTime64_t (0xC995DE02). They use
  the 16-byte guest calendar and fixed 64-bit unsigned Unix seconds; calendar
  conversion does not depend on native time_t width, local timezone or DST.
- Calendar conversion supports 1970-01-01 through 9999-12-31 23:59:59. Pre-epoch
  unsigned wrap behavior is not implemented. Invalid calendar fields return RTC
  field errors, out-of-range epochs return INVALID_VALUE, and failures preserve
  outputs. GetTime64_t discards subsecond precision; SetTime64_t sets it to zero.
- CurrentClock preserves gettimeofday microseconds and maps backend failure to
  NOT_INITIALIZED. Nonzero timezone follows the pinned Vita3K hour interpretation;
  the VitaSDK user header does not specify units. Firmware behavior for nonzero
  zones remains unverified; host timezone settings are not used.
- Host tests check NID dispatch, Gregorian leap centuries, epoch/2038/year9999
  boundaries, 2000 independent gmtime_r oracle samples, invalid fields, null
  pointers, failure preservation and a clock-zone day/month rollover. A bounded
  local Bonsai outline suggested test categories but had incorrect epoch examples;
  no generated constants were accepted without checking. The full ASan/UBSan
  host suite and ARM Debug/Release builds passed.
- No RTC clock-setting function changes the Switch clock: SetTime64_t only
  converts a supplied epoch into a date. No Switch files or runtime were changed;
  hardware library startup and commercial game boot remain unverified.

## Offline thread-info snapshots (2026-11-27)

- Added public sceKernelGetThreadInfo (SceLibKernel 0x8D9C5461) and raw
  _sceKernelGetThreadInfo (SceThreadmgr 0xB373D8A1). Both accept a thread ID
  (zero selects the caller) and a pointer to the fixed 128-byte guest structure.
  Invalid size, unknown ID and null output fail without overwriting the output.
- The snapshot includes the process UID, bounded name, guest entry address,
  native executable stack mirror/usable size, original resolved Vita priority,
  current priority and published exit result. Fixed-width layout assertions cover
  size and pointer/clock offsets on host; ARM also checks the SDK structure.
- Stack selection follows libnx threadCreate: it passes the mapped stack mirror
  to svcCreateThread and reserves entry bookkeeping above its usable stack size.
  Source: https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/thread.c
- Snapshot lookup/copy shares the thread-registry lock with priority changes and
  deletion. Exit result and completion publication are atomic. Host tests cover
  both NIDs, descriptor size, pointer offsets, running/completed lifecycle,
  output preservation, and querying while priority changes race with deletion.
- Status is a coarse lifecycle view: unstarted/completed threads are DORMANT,
  the querying live thread is RUNNING, and other live threads are READY. Detailed
  wait/suspend state, CPU placement, affinity and scheduler counters are not
  tracked: CPU IDs are -1 and other unsupported fields are zero. Attribute and
  affinity creation options remain unsupported rather than newly implemented.
  This query does not prove full scheduler introspection or hardware startup.
- The full ASan/UBSan host suite and ARM Debug/Release builds passed.
  No Switch files were transferred or tests requested while the user was asleep.

## Offline thread-priority changes (2026-11-27)

- Added SceThreadmgr sceKernelChangeThreadPriority (0xBD0139F2) and
  sceKernelChangeThreadPriority2 (0xC5FFCA61). The former returns zero on
  success; the latter returns the previous resolved Vita priority, following
  the pinned reference implementation. Thread ID zero selects the caller;
  priority zero copies the caller's priority even when targeting another thread.
- Supported Vita/default priorities use the existing 64..191 to Horizon 28..59
  mapping and call svcSetThreadPriority on the native thread handle. A failed
  native update preserves guest priority and TLS. Priority queries retain the
  resolved Vita value, including values sharing a native scheduling bucket.
- Priority is atomic for concurrent readers. Updates and DeleteThread serialize
  native-handle use, closing, registry removal and record/TLS reclamation under
  the registry write lock. This specifically protects priority-update/delete;
  other thread operations still borrow records and need broader lifetime work.
- Kernel TLS word 8 is initialized and updated with priority, matching the pinned
  kernel TLS index. Host checks cover NID lookup, self/other targets, zero/default
  priorities, native mapping, previous-value returns, invalid values, backend
  failure preservation, and a real pthread priority-update/delete race. The full
  ASan/UBSan host suite and ARM Debug/Release builds passed.
- This is native scheduler plumbing with host-model checks, not a claim of Vita
  scheduler timing parity or a hardware-tested FIOS/library/game startup.
  No Switch files were changed while the user was unavailable.

## Offline texture gamma sampling (2026-11-27)

- Added sceGxmTextureSetGammaMode (0xA6D9F4DA), using the pinned descriptor's
  two-bit field and shift-by-27 operation. Null texture returns INVALID_POINTER;
  conversion support is checked when building the native image layout.
- Gamma BGR now selects native sRGB RGBA8 and BC1/BC2/BC3 images, including
  existing upload and mip-chain paths. The packed upload bytes, source geometry,
  channel swizzle and row padding remain unchanged; sampling performs conversion.
  This follows the pinned Vita3K renderer's native sRGB color-texture path.
- Both the pinned VitaSDK and Vita3K headers alias GAMMA_R and GAMMA_BGR to
  0x08000000. This change does not guess new enum values. Single/two-channel,
  signed/float and channel-selective GR conversion remain unsupported, with
  unchanged output metadata on conversion failure.
- Host checks cover descriptor-bit preservation, all four native sRGB format
  selections across three mip levels, byte-for-byte packing parity, unsupported
  combinations and null input. The full ASan/UBSan host suite and ARM Debug/Release
  builds passed. These checks establish metadata/packing behavior, not GPU color accuracy.
  No files were transferred to the Switch; commercial boot remains unverified.

## Offline front point/line width (2026-11-27)

- Added sceGxmSetFrontPointLineWidth (NID 0x06752183), imported by the supplied
  LittleBigPlanet executable. The SDK declares an unsigned integer pixel width;
  it is converted to float internally for deko3d, with no guest VFP adapter.
- Context state defaults to width 1. Changes are replayed before the next draw,
  and BeginScene marks the state dirty so another context cannot leave its
  native point/line width active. Both native point size and line width are set,
  following the pinned Vita3K GL renderer's front-width behavior.
- Separate back-face widths and exact hardware width clamping remain unsupported
  or unverified. This does not add polygon-mode or point-sprite UV support.
- Local Bonsai source summarization exceeded its context limit; no generated
  patch was used. Source review and ARM Debug/Release builds provide integration
  checks; rendered size and rasterization behavior require a future GPU test.

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
