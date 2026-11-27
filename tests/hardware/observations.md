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
