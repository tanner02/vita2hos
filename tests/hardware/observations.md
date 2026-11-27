## 2026-11-27: shader-tail correction renders real startup; counted wait added

Private verification commitabc5a95 combines unchanged owner073ee41 HLE with
Codexcfd9616's allocator/test/stub changes only. Native Debug compilation,
source-archive byte checks, ARM32/runtime identity and three-NCA verification
pass. On Switch, the original game gets past the former4096-byte shader fatal,
compiles hundreds of shaders and renders Sony startup text followed by the
animated autosave/loading screen. Black rectangles and other rendering artifacts
remain visible. The loading run continues compiling; a title menu or interactive
3D scene has not been established. Live UDP capture is best-effort and has
missing/interleaved lines; it is used for progress, with screenshots as visual
evidence. The shader fix is hardware progress, not a complete-game claim.

One observed missing import is library5ed8f994/NID7d9864a8, called with vcount2.
The pinned SDK identifies sceDisplayWaitSetFrameBufMulti; the pinned Vita3K
reference waits until last-framebuffer-set vblank count plus vcount. The new
noncallback export uses that snapshotted target, including immediate completion
when already elapsed or count0, and the existing shutdown-generation checks.
It rejects unrepresentable64-bit targets before addition. Later framebuffer
submissions cannot move an in-progress wait's target. Existing single-period
wait behavior is preserved. Callback waits and per-thread VblankMulti are not
added by this change.

Host ASan/UBSan thread checks verify two ticks without an extra tick, elapsed
and zero waits, framebuffer resubmission during a wait, large-count cancellation,
shutdown/restart isolation, target overflow and existing snapshot/predicate tests.
No native or hardware result is claimed for the new counted export yet. A local
readonly worker draft added an incorrect extra tick; it was rejected rather than
applied. HLE owner source files and the held real-libc TLS probe remain untouched.

A button-injection attempt during loading failed with HID result1c24ca; neutral
and detach requests returned the same error. Screenshots and network logs still
work. No reboot or game restart was issued for that failure. The existing game
is preserved while checking input-session recovery and preparing later builds.

## 2026-11-27: real title reaches shader compilation, reserved code tail fixed

The unchanged frozen073ee41 HLE runtime first passes the entire owned cube7
batch:35 PASS tags, all360 cube frames, process0 and image cleanup0. Original
LittleBigPlanet files and the newly restored index then ran on that runtime.
A Windows multicast receiver captured live Switch netlog without opening the
running title's SD log. The game reaches actual shader translation and host UAM
compilation, then deko3d rejects a4096-byte DKSH at dkShaderInitialize because
its code extends into DK_SHADER_CODE_UNUSABLE_SIZE at the block's end. The
application error dialog was closed remotely; HOME/Start shows it closed.

The allocator previously rounded only the DKSH byte count to a page. It now
adds deko3d's required1024-byte unusable tail before page rounding, rejecting
sizes that would overflow either addition. Copy and zero-fill still cover the
payload and all remaining allocation bytes. The installed SDK header defines
the constant, and [deko3d's admission check](https://github.com/devkitPro/deko3d/blob/master/source/dk_shader.cpp)
requires code-end <= block-size minus that tail. A page-sized blob therefore
needs a larger block; smaller sample shaders happened to fit the old padding.

Host ASan/UBSan checks pass for page/tail boundaries, the real4096-byte case,
copy/padding preservation, near-UINT32_MAX rejection before allocation and
existing failure/cleanup ownership. Hardware verification of this correction
is pending. No commercial 3D scene is established yet. HLE code remains with
its owner; the correction changes only Codex's shader allocator and host tests.
Detailed shader code, game logs and source assets remain private.

## 2026-11-27: missing game resource index restored and content verified

A fresh SD listing confirmed that the original game payload contained data.farc,
gamedata and sce_sys but lacked the entire output directory. The owned source
contains output/blurayguids.map (1293883 bytes) and output/titleid.txt (9 bytes);
the executable references both paths. These two absent files were added under
the application's SD root and exact-readback verified. Existing files were
preserved. No proprietary contents are committed to the repository.

The source GUID index parses to21798 records and exact EOF. Its21792 archive
SHA1 references match the source FARC index; the six remaining entries match
external font files. All six SD fonts match source bytes. A full streaming SD
archive SHA256 equals the current source archive, not just its size:
0545397887141584ac23f9cbc172638e0ef75727fda0dae18714ee25f177dba1.
This establishes file completeness/integrity for those indexed resources. It
does not establish that the guest reached these paths, that the index caused
the previous resource fault, or that restoring it boots the title.

The first newly labelled owner HLE artifact failed independent runtime-identity
verification: metadata says c389222 while decompressed NSO rodata says
21adb05-dirty with an older compile timestamp. That stamp alone does not prove
all functions are old, but the candidate was not installed as verified c389222.
The clean c389222 host suite passes; its native target declares two missing
source files, restored in owner commit073ee41. A live-source build was stopped
when owner tracked files changed during configuration, rather than attributing
its slow Git dependency scan to a compiler failure.

Codex exported immutable owner073ee41 source and built it in a separate private
build directory with four verified-pinned read-only dependency binds. Every
exported source file remains byte-identical after compilation. A build-only
revision wrapper supplies the independently verified archive commit because the
export has no .git; no HLE source or repository CMake was edited. Final native
ELF and packaged NSO identify073ee41. ARM32 metadata, exact NSO preservation,
all three NCA hash trees and CNMT hashes pass. HLE remains unmerged into Codex
main. Hardware verification of that candidate is ongoing; no game-boot result
is claimed here. The held real-libc TLS probe remains unchanged.

## 2026-11-27: log-retry runtime passes graphics batch with live collection

Clean Debug/Release builds of927c50f pass the existing build/package checks,
including all three NCA hash trees and ARM32 process metadata. The dedicated
HOME title was installed and the same owned visible_cube7 batch ran again.
All35 checks pass, including ten numeric vertex formats and all360 cube frames.
Screenshots show the cube followed by HOME/Start with the title closed.

Thirteen live log-read attempts were made during rendering. Six of the first
twelve returned HTTP404 despite the file existing; successful reads continued
growing. The final96316-byte log identifies927c50f and contains process result0,
launch result0 and image cleanup0. This establishes successful collection through
the end of this run. It does not identify the native filesystem errno, force an
append-open failure, prove every message survived, or establish that prior file
reads caused the earlier truncation. The host injected-open-failure regression
fails against the old source and passes under ASan/UBSan against927c50f.

Runtime-log SHA256: 235122a514ddc78be263126dc711d3b33766ac7ba8cef60378819da79dd7b17d.
Report SHA256: 36eecef77472a3931515d78c78750ccfc5d9cd1ebaf6e3453514949947305a0b.
Original LittleBigPlanet executable/modules/normal mode were restored and
readback-verified afterward; installed/root runtime927c50f remains. Virtual
input is detached. No HLE/scanner integration, global override change or held
TLS-probe update occurred. Commercial-title boot remains unverified.

## 2026-11-27: wider fragment-shader compile audit and log-open recovery

A deterministic sample of520 owned-game shaders was selected from21294 unique
archive programs, grouping by program flags and spreading each group across
parameter-count/size order. Every selected program translated through the
production wrapper and compiled through host UAM to structurally valid DKSH.
The current pinned-fixture integration checks also pass. All selected programs
are fragment shaders; this result does not cover commercial vertex programs,
vertex binding hints, GPU execution or title boot. The archive's single raw
GXP magic occurrence was inside a video and failed basic size validation; it
was not counted as another shader. Assets and detailed diagnostics stay private.

During cube7 collection, concurrent file reads returned temporary access errors,
and the runtime log stopped mid-render although the guest completed all35 checks
and returned HOME. The exact failing native filesystem operation/errno was not
captured, so read/write contention is a hypothesis. Source inspection establishes
that any append fopen failure permanently disabled the file sink. The logger now
retries with the next message when opening failed before any write. It still
disables after failed rotation or write/close, where ownership or byte accounting
may be uncertain. The failed message itself is not replayed.

Fault-injected host ASan/UBSan checks verify recovery without rotation and after
successful rotation followed by a failed append open, unchanged bytes on the
open failure, and preservation of prior bounded-log/failure checks. No new
hardware log-recovery result is claimed yet. HLE ownership, held TLS probe and
restored original game remain unchanged.

## 2026-11-27: ten numeric vertex formats verified in one hardware batch

Visible cube7 adds actual textured draws for U8, S8, U16, S16, U8N, S8N,
U16N, S16N, F16 and F32 UV attributes. Each vec2 has a nonzero byte offset,
poison padding and a stream separate from F32 positions. Signed formats include
negative values; normalized formats encode unit endpoints; raw integer formats
encode numeric unit endpoints. A fullscreen indexed quad samples a point/clamp
quadrant texture at four interior locations. Each case changes the texture
palette and resets the guest framebuffer to a distinctive untouched value.

On installed runtime8bf8795, all ten new PASS tags appeared, followed by all360
cube frames, intact interior ring-buffer guards and final PASS visible_cube.
The complete report contains35 PASS tags, including the existing MSAA, depth,
stencil, gamma, texture-update and viewport checks. A screenshot shows the
textured cube; a later screenshot shows HOME with Start and the title closed.
Report SHA256: 36eecef77472a3931515d78c78750ccfc5d9cd1ebaf6e3453514949947305a0b.
The captured runtime log identifies8bf8795 but ends during cube rendering; it
does not include process-result or cleanup-result lines. Closure is established
by the final report and HOME screenshot, not a captured exit-status value.

Both draw10 and cube7 build with warnings treated as errors; their import audits
verify60/61 current source exports. No runtime implementation or NSP changed.
This establishes the tested vec2 numeric fetch paths; it does not establish
UNTYPED/register inputs, vertex arrays, matrix layout or commercial-game boot.
Original game files are preserved separately from these owned test artifacts.

## 2026-11-27: startup rollback identified before the cleanup fault

Read-only analysis of the owned main executable and real libc links the latest
8bf8795 run to the LiveAreaManager initialization stage. Its initialization
callback returns false when loading LIVEAREA (module0x23) fails; the log records
that load returning805a10ff. The startup framework calls the stage callback,
checks its boolean return, clears the startup-success flag on zero and invokes
the stage finalizer. The bringup result reaches main, which returns0; the CRT
passes that status to libc exit. This explains the observed NP activity cleanup
and exit-handler calls without treating their proximity to the crash as cause.

The observed AbortHandler argument0x102 selects libc's exit-status-zero path.
Both missing AbortHandler and ModuleExit return values are discarded before
__cxa_finalize(mainDSO). Making those two calls return success alone would not
remove the preceding LIVEAREA rejection or establish valid teardown state.
The subsequent null registry-table access remains a separate cleanup defect.
The saved native thread mapping and TPIDR agreement do not establish TLS/DTV
contents. Main replaces argc/argv immediately, so zero launch arguments are not
established as a startup cause. Private source-bound receipts and original
assets remain outside the repository; no guest ARM code was executed on PC.

The HLE owner received these findings through AGENT-PROGRESS.md. Its proposed
LIVEAREA support remains unmerged, as does the rejected unbound-stub scanner.
Codex has not changed HLE files or the held real-libc TLS probe. Commercial-title
boot remains incomplete.

## 2026-11-27: real-game startup on the verified graphics runtime

Original LittleBigPlanet PCSF00516 ran on8bf8795 after its owned graphics batch
passed. The first cycle exhausted64 address-layout retries; Codex closed the
observed error dialog and relaunched. The second cycle found a compatible
layout after13 restarts and reached guest entry. No payload/profile change was
needed to restart the counter. A source-based placement model explains why this
fixed window is difficult; it does not prove uniform independent hardware odds.
The32-bit-without-alias profile folds alias size into heap, so it is not simply
extra free code space and was not installed as a workaround.

The new main-thread fault is PC0x810461b8, LR0x813a39ef, effective address0x34.
Thread mapping identifies native0x31e as guest UID5, <main>. The decoded10-thread
DTI2 matches every crashed register/PC/LR/SP and exact EOF; saved TPIDR0x8dd058b0
matches that thread's recorded user-TLS pointer. This does not prove TLS/DTV
contents. Source-bound decoding loads entry3 from a zero registry-table pointer.
Unsupported AbortHandler and ModuleExit calls precede the fault; failed startup
followed by teardown is a hypothesis, not an established root cause. This differs
from the older0x813a3f28 resource-TLS fault on the unmerged HLE runtime. No shader
creation failure tag was captured in this run; absence is not proof of success.

Crash report SHA256: 2a7ff8d9af22e99d264ff76c9e8d3b54eabe93d051e0e932fecfd73439fe49f3.
Complete logs, thread dump, SDK-resolved unsupported-call list and receipts remain
private. Detaching virtual input immediately after launch allowed a fresh input
attachment to close this crash dialog without another reboot. HOME showed the
title closed. Original game/modules/normal mode and root runtime8bf8795 were
hash-verified afterward. No HLE/scanner merge or held TLS-probe update occurred.
Commercial3D boot remains open.

## 2026-11-27: graphics batch hardware PASS on8bf8795

Automated visible_cube6 ran through the dedicated HOME title on firmware16.1.0,
Atmosphere1.8.0. All25 PASS tags were observed, including2x/4x transfer
preservation and triangle coverage/depth/resolve, restored1x coverage, gamma
RGB/alpha/reset, depth-only occlusion, stencil replacement, six texture/viewport
passes, all360 cube frames and interior ring-buffer guards. A screenshot shows
the textured cube. Runtime8bf8795 logs process result0 and cleanup0; the title
returned to HOME with Start displayed and no GPU error in the successful log.

Report SHA256: 23783e212bb74bc0f3f796d4602b794f0567eb4ccf884af9881d0328112d6b73.
Runtime log SHA256: 8d9e166f7a52d4952ee34f182849ad0a4aa5b9eba7c2db992658d14c1a08f63d.
Private receipts retain the installed package/readback hashes, complete report,
log and screenshots. No proprietary game assets are included in those graphics
artifacts. This validates the owned batch, not commercial-game boot, vertex
array/matrix rendering, exact Vita sample positions, raw sample storage,
gamma-correct MSAA resolve or forced multisampled depth transfers.

The first hardware attempt on a421853 failed before the first framebuffer pass:
GPU methodb197:3ef receivedffffffff. The method maps to MultisampleSampleMask;
[deko3d queue setup](https://github.com/devkitPro/deko3d/blob/master/source/maxwell/gpu_3d_base.cpp)
initializes those methods toFFFF.8bf8795 restricts the runtime's write toFFFF;
the rerun above passes. The crash also invalidated the attached virtual input
handle. Game files were restored before controller-recovery reboot; the user
brought the console back. Subsequent launches detach the virtual controller
immediately after input. Recovery across another crash remains unverified.

Original LittleBigPlanet executable/modules/normal mode were restored and
hash-verified after the successful batch. The real game is now being observed
on the same installed runtime. No HLE handoff/scanner is merged, and the held
real-libc TLS probe remains untouched.

## 2026-11-27: multisample draw/depth batch checks (not deployed)

The existing graphics batch now tests actual draws in2x,4x and restored1x modes,
in addition to its no-draw color transfer preservation. A near green triangle
occludes a later far red fullscreen triangle. Interior and exterior readbacks
check depth ordering. Eight oblique-edge pixels must contain red/green mixtures
for MSAA and binary red/green for1x. Every target uses a matching fragment-program
mode, native transient depth, no gamma and downscaled color where applicable.
Temporary shaders/targets are retired after Finish; original geometry/program
state is restored before the existing depth, stencil, texture and cube checks.

The edge is x+2*y=72.5; selected integer pixels satisfy x+2*y=71. Exact rational
geometry checks split native default2x/4x positions evenly under all axis
reflections. A private host harness extracts the actual guest pixel checker:
ASan/UBSan accepts valid mixtures and single-sample coverage, and rejects missing
coverage/depth, stale multisampling, invalid blue/alpha and inconsistent channel
sums. This verifies the oracle, not GPU behavior. A lightweight local review had
arithmetic errors; those values were not used and were checked independently.

Draw probe9 and visible cube6 compile with VitaSDK warnings treated as errors.
Their entry/import audits verify60 and61 current runtime source exports. No new
native runtime code changed in this commit, and no hardware result is claimed.
No staging, deployment, console access or held real-libc TLS probe update occurred;
ZCode retains console/HLE ownership. Commercial3D boot remains unresolved.

## 2026-11-27: native multisample attachments and color resolve (not deployed)

Render targets previously ignored their multisample mode and always created
single-sample images. Forced depth load/store already rejected multisampling.
Default2x/4x render targets now create matching native multisample color/depth
attachments and a single-sample color resolve image. BeginScene sets matching
rasterizer/sample modes, native default sample locations and a full sample mask;
returning to1x explicitly resets that state. EndScene resolves to an equally
sized image before copying/scaling to the guest color buffer. Full barriers order
rendering, resolve and the subsequent copy. Single-sample targets avoid the extra
resolve allocation. Failure at any allocation step rolls back all owned images.

The supported color route is linear ABGR8 with MSAA_DOWNSCALE and no gamma.
Raw per-sample color storage, custom sample locations and gamma-correct resolve
return unsupported before scene mutation. Multisampled forced depth transfers
remain unsupported. Native default sample positions are not established as an
exact Vita match. This is not complete GXM multisample compatibility.

Host ASan/UBSan checks verify image types/sample modes, attachment/resolve setup,
rollback at every allocation stage, view ownership, resolve dimension/sample
validation and command ordering. The full host suite and native Debug/Release
builds pass. The owned graphics batch adds no-draw load/resolve preservation at
2x and4x followed by1x, verifying both active and inactive guest buffers. Surface
probe3, draw probe8 and visible cube5 compile; the visible-cube entry/import audit
verifies61 current source exports. No new hardware report exists. These transfer
cases do not prove draw coverage, sample positions or antialiased geometry.

Review of [deko3d image transfer implementation](https://github.com/devkitPro/deko3d/blob/master/source/dk_image.cpp)
(blob883b0aa4778d1a4c8b5955de157d04e9b81732ac) confirms that blit scales
multisampled destination storage while resolve requires an equally sized
multisampled source and single-sample destination. Local SDK headers and native
builds check the APIs used here; GPU behavior remains unverified.

Console remains ZCode-owned; no Codex hardware access, staging, installation,
launch or held real-libc TLS probe change occurred. HLE integration stays with
ZCode. Startup failure and commercial3D boot remain unresolved.

## 2026-11-27: numeric vertex array groundwork (not deployed)

Non-register vertex arrays now reserve one native input location per array
element. Previously the generated translator/analyzer allocation rounded the
flattened component count to vec4 slots, which could overlap the following input
for scalar, vec2 or vec3 arrays. Runtime fetch also previously configured only
the first location. Packed register inputs retain their separate vec4 packing.

Runtime metadata carries the array element count. Native fetch expands the guest
attribute format/component count at consecutive locations and advances offsets
by the actual storage byte width. Normalization and integer/numeric conversion
are preserved. Invalid metadata, collisions, native capacity and offset overflow
fail before replacing output state. Original dependency sources stay unchanged;
the guarded CMake patch modifies generated translator/analyzer copies only.

Twenty-four declaration derivatives cover float and signed-integer arrays with
one to four components and two to four elements. Actual SPIR-V reflection checks
type, vector width, location span and no overlap with adjacent inputs; all cases
also convert through the runtime GLSL wrapper. Host ASan/UBSan checks cover all
eleven guest formats at widths1..4 plus conversion and failure preservation.
The full host suite, shader integration test and native Debug/Release builds
pass. No GPU execution, actual matrix memory layout or rendered game geometry
is established. The original startup crash remains open; HLE integration stays
with ZCode. No console access, payload/probe change or deployment occurred.

## 2026-11-27: latest saved startup crash and thread-pointer evidence

After the user enabled Switch access, read-only collection retrieved three saved
crashes with the same guest PC0x813a3f28, LR0x81044a01 and fault0x158. The latest
log identifies runtime1c1c20c-dirty. Original game and SD runtime archive hashes
were verified; HOME showed the title closed. No title input, launch, installation,
SD write or held probe correction occurred. Hardware ownership was released.

The latest DTI2 thread dump contains24 threads. A private bounded decoder based
on Atmosphere1.8.0-prerelease checks unique IDs, exact EOF and every crashed-thread
register against the text report. All checks pass. The upstream writer stores
stack-trace count bytes rather than count times8 bytes; those partial trace bytes
must not be interpreted as complete return addresses.

Crashed native thread0x457 has saved TPIDR0x8e1058b0, distinct from the HOS TLS
address0x12229800. Atmosphere's exception handler saves TPIDR_EL0, and the ARM32
debug-context path copies its low32 bits. The value is nonzero and consistent
with retargeted guest user TLS. This narrows a zero-register hypothesis but does
not establish allocation/template, DTV or current-resource contents. No live
memory at that pointer was captured. Resource readiness and TLS publication remain
unresolved; no successful 3D-title boot is claimed.

The SD native archive's build ID is nonzero while the crash native module ID is
zero, so exact archive-to-crashed-code identity is not established by build ID.
The archived code at the captured native accessor offset resembles the kernel TLS
getter, but this alone cannot prove an executed symbol or a resolver defect.
Codex thread/shader diagnostics remain undeployed. HLE integration stays with
ZCode, and the unsafe unbound-stub scanner remains excluded.

Format/ABI sources: [DTI2 writer](https://github.com/Atmosphere-NX/Atmosphere/blob/1.8.0-prerelease/stratosphere/creport/source/creport_threads.cpp),
[context layout](https://github.com/Atmosphere-NX/Atmosphere/blob/1.8.0-prerelease/libraries/libvapours/include/vapours/svc/svc_types_common.hpp),
[ARM32 debug context](https://github.com/Atmosphere-NX/Atmosphere/blob/1.8.0-prerelease/libraries/libmesosphere/source/arch/arm64/kern_k_debug.cpp),
[exception register save](https://github.com/Atmosphere-NX/Atmosphere/blob/1.8.0-prerelease/mesosphere/kernel/source/arch/arm64/kern_exception_handlers_asm.s).

## 2026-11-27: high-ID sysmodule handoff audit (offline)

The reviewed HLE sysmodule implementation tracks loaded stub modules in one
uint64_t but accepts public IDs above63, including NP_SIGNALING0x46. Its load,
query and unload paths shift by the raw ID. A host UBSan check stops on shift
exponent70. A private draft replaces the mask with a bounds-checked state array;
high-ID load/unload and independence from low-ID FIBER pass on that draft.

Codex's current sysmodule implementation has no such mask and remains unchanged.
The host network/sysmodule regression now exercises lifecycle consistency and
low-ID isolation across public IDs64..JSON; ASan/UBSan passes. This guards a later
integration. The private draft changes state representation only; it does not
validate the handoff's stub-load whitelist or implement its services.

The user keeps HLE edits and integration with ZCode; Codex leaves that lane untouched.
Other review limits include inline fiber execution without real yield/context
switching and dialogs reporting success without a corresponding host operation.
No complete HLE integration or successful 3D-title boot is claimed. No hardware,
probe, payload or deployment changes occurred.

## 2026-11-27: import-pool handoff audit (offline)

A proposed second-pool scanner is excluded from this branch. Source-bound replay
of the owned main ELF finds631 declared function imports and630 NOP/zero/MVN/BX
windows. Each window crosses two standard16-byte import stubs: a stub's trailing
NOP/zero words and the next stub's leading MVN/BX words. Every window+8 is already
a declared import entry, so these windows do not establish another unbound pool.

Host ASan/UBSan replay using the production relocation and import-resolution
functions with synthetic export addresses removes all630 patterns. The addresses
are test providers, not proof that real services are implemented or that the game
boots. A new loader regression checks adjacent standard stubs, declared entry
patching and preservation of unlisted matching bytes. No guest ARM is executed.

The reviewed scanner also emits ARM LDR pc,[pc,#0] while placing its target in the
adjacent word. ARM's PC+8 addressing loads the wrong word from either proposed
entry. Its only hook is load_file, whereas the normal app/process route binds
images directly through load_image_bind. These findings contradict the proposed
offline crash-fix claim; no scanner or HLE handoff is merged here. Other source
and service behavior still needs review. The real startup crash remains open.
No console/probe/payload change or deployment occurred during this audit.

## 2026-11-27: shader admission failure diagnostics (not deployed)

Private source-bound import/control-flow decoding links a resource-construction
helper to sceGxmShaderPatcherCreateFragmentProgram. Registration and vertex or
fragment creation can fail before the existing shader-conversion log. Therefore,
absence of conversion messages does not establish that creation was never called.
An admission failure is a hypothesis; no captured return value proves it caused
the game's current-resource TLS value to remain unset.

Failed registration/creation now logs operation, phase, opaque program pointer
and the original SDK result. Tags distinguish output/argument/registry/header,
allocation, uniform/vertex layout, input metadata, blend and translation failures.
The atomic budget admits at most64 failures per GXM initialization plus one
exhaustion notice. SDK results, output clearing and cleanup paths are preserved.
An eight-thread ASan/UBSan check verifies the budget, reset and return preservation;
native Debug/Release builds pass. No game failure tag has been observed yet.

After ZCode's recorded handback, a read-only sys-autopilot status attempt failed
to connect; no fresh log/crash was collected, and the console claim was released.
No title/input/install, SD payload change or probe correction occurred. The probe
hold remains. Actual startup failure and commercial 3D rendering remain open.

## 2026-11-27: startup thread attribution (offline, not deployed)

Atmosphere crash Thread IDs are native kernel identifiers; the existing Created
thread log prints guest UIDs allocated by SceSysmem. Comparing those counters
cannot establish that many guest threads were created after the log tail. Thread
creation now logs both IDs, bounded thread name, entry address, kernel/user TLS
addresses and template sizes before starting the native thread. Native-ID query
failure is recorded explicitly and does not change the guest creation result.

Read-only follow-up on the saved42bf233 crash confirms the faulting callee has
not changed r8 before the data abort; its nonzero value is a resource-object
candidate from the caller's resource selection, not evidence for a float value.
A related bind setter publishes the current resource TLS pointer only for ready
states4/5; its other branch requests resource work and returns failure. This
supports investigating readiness/publication separately from TLS address
instability. It does not identify the executed branch or resource state.

Thread-runtime and module-start ASan/UBSan checks pass, and Debug/Release compile
the native SDK query successfully. No hardware mapping output has been observed
yet. Source-bound ELF/disassembly/crash evidence stays private; no guest ARM code
was executed on the host. The real-libc probe correction remains held, and no
console/SD access, payload changes or deployment occurred. Startup crash and
commercial-title 3D rendering remain unresolved.

## 2026-11-27: C10 packed-input location allocation (offline, not deployed)

Register C10 declarations now reserve native locations from their packed-byte
count in both the shader translator and metadata analyzer. Previously a four-
component C10 input occupied two native locations but reserved only one, so a
following input overlapped it. Generated build copies carry the correction;
the pinned Vita3K checkout and submodule revision are unchanged.

Controlled pinned vertex derivatives put one-, two- and four-element C10 arrays
before another named input. Actual SPIR-V reflection verifies non-overlapping
spans, adjacency and agreement with runtime metadata; runtime GLSL conversion
also passes. This regression fails with the original allocation. A private
configure fixture verifies failure after changing or duplicating the expected
upstream source statement. A second build reports no work, and native Debug/Release builds pass.
The patch composes with the earlier uniform-shuffle correction in both builds.

These are shader declaration/translation checks, not GPU execution or proof of
C10 rendered pixels. Non-register matrix/array support and the guest startup
NULL+0x158 crash remain open. No commercial 3D boot is established. Probe update
remains held; no console/SD access, payload changes or deployment occurred.

## 2026-11-27: packed register vertex fetch (offline, not deployed)

Register-format vertex inputs now fetch unsigned bit patterns at shader-declared
8/16/32-bit widths, bypassing ordinary signed, normalized and float conversion.
Shader array elements flatten into bounded native locations with byte offsets;
partial final vectors select only their remaining elements. State creation rejects
collisions, slot exhaustion, unsupported widths and offset overflow atomically.

Real pinned fixture derivatives cover F16, signed-byte and F32 register arrays,
plus three-component C10 packing. SPIR-V reflection checks unsigned input type,
vector/array shape and location; runtime GLSL conversion is also required. The
full ASan/UBSan host suite and native Debug/Release builds pass. These are parser,
translation and state checks, not GPU bit-pattern or geometry validation.

A remaining pinned-translator limitation: C10 packed-byte expansion may occupy
more locations than its logical component allocation reserves, overlapping a
following input. The runtime rejects overlapping native states; the translator's
allocation still needs correction. Non-register arrays/matrices are also open.
No 3D-title boot is established. User probe hold/ZCode console ownership remain;
no probe correction, console access, SD changes or deployment occurred.

## 2026-11-27: declared vertex input locations (offline, not deployed)

Vertex fetch now uses the shader declaration location associated with each guest
register rather than the caller's attribute-list position. Unmatched attributes
in named shaders are skipped. Stripped shaders retain hint-list positions and
float-input conversion. Native attribute state is checked and cached at program
creation; unused slots are fixed, and only referenced streams require buffers.
Duplicate and out-of-range locations fail without publishing partial state.

Real pinned GXP/SPIR-V reflection verifies input names and locations. A controlled
stripped derivative with reversed hints verifies fallback locations and float
input types. Host sanitizer tests cover reordered/sparse bindings, unused slots,
empty programs and error preservation. Native Debug/Release builds pass. These
are declaration/state checks, not GPU execution; packed registers, matrices and
arrays need further work. No commercial-title boot or rendering is established.
User probe hold and ZCode hardware ownership remain active: no console access,
probe correction, payload change or deployment occurred.

The unused-slot strategy follows deko3d's fixed-attribute fallback:
https://github.com/devkitPro/deko3d/blob/master/source/maxwell/gpu_3d_vbo.cpp

## 2026-11-27: numeric vertex attribute conversion (offline, not deployed)

Raw U8/S8/U16/S16 vertex attributes now use deko3d numeric float conversion
when the pinned shader parser declares a floating input. Integer and raw-register
inputs retain integer fetch. The classification is cached during vertex program
creation; stripped shaders follow the translator's float-input hint fallback.
Normalized, float and untyped32-bit format mappings retain their existing behavior.

The full ASan/UBSan host suite, real pinned GXP metadata integration checks and
native Debug/Release builds pass. Metadata-only fixture derivatives cover float,
integer and raw-register declarations. Input locations, packed-register handling
and rendered geometry still need further work and hardware validation. No game
boot is established. User probe hold and ZCode hardware ownership were respected;
no console access, probe correction, payload change or deployment occurred.

## 2026-11-27: untyped vertex fetch support (offline, not deployed)

Vertex layout admission now accepts SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED. Its
one-to-four components select32-bit integer fetch, preserving raw register
values without normalized/float conversion at fetch time. Existing typed
format mappings are retained. The raw32-bit treatment follows the pinned
reference renderer's untyped integer input path.

Vertex format conversion now returns SDK errors for invalid formats/counts or
output pointers instead of invoking unsupported-format abort helpers. Host
regressions cover all44 format/component combinations and output preservation.
Shader-declared type/location mapping, packed register-format attributes and
actual GPU fetch remain additional work; no commercial rendering is established.
No probe update, deployment or console access occurred during this change.

## 2026-11-27: vertex sampler binding correction (offline, not deployed)

The initial vertex-texture implementation used stage-local bindings0..15, but
the pinned GLSL translator declares vertex samplers at16..31. Corrected native
binding calls to retain that offset and share the same index calculation as the
descriptor snapshots. Fragment bindings remain0..15. This fixes a mismatch in
Codex's earlier vertex-texture implementation; no GPU pass was claimed for it.

Pinned shader integration now checks all16 named sampler declarations for both
stages against the shared runtime index. Controlled copies are declaration-only.
Separately, host UAM1.1.0 compiled owned shaders that actually sample at vertex
bindings16/31 and fragment0/15; TGSI retains the corresponding sampler/view
indices. This verifies compiler acceptance and the binding contract, not pixels.
The ordinary texture ownership/bounds host regression passes.

No console access, probe update, package installation or target launch occurred.
The correction is intended for a later combined hardware batch after ZCode's
current work. Additional vertex format/reflection compatibility remains open.

## 2026-11-27: automated real-title run past LIVEAREA, guest crash captured

Using authenticated sys-autopilot1.6.0, installed the provided ZCode42bf233
HOME package after verifying its full package and three NCA hashes. Installation
returned the dedicated title ID0100f27432680000. Source remains unmerged; the
package's dialog success stubs remain an explicit compatibility limitation.
The original guest/assets/modules were preserved.

The title found a compatible layout after21 restarts. LIVEAREA load returned
success and the game created resource-loading, main and glyph threads beyond
the prior teardown point. No first-use unsupported call, shader compilation or
scene submission was logged after guest entry. An unchanged log did not prove
that threads were still waiting: Atmosphere recorded a guest Data Abort and
later displayed its software-closed error screen.

Private crash report PC813a3f28 reads a halfword at r0+0x108 with r0=0x50.
Read-only source-bound Thumb decoding traces the caller through the real
libc __tls_get_addr import; the resulting thread-local object pointer is null.
This identifies the failing use, not the reason the object was uninitialized.
An owned real-libc TLS address/initialization/isolation probe is the next check;
raw thread-pointer tests alone cannot validate this library path.

Default-layer screenshots work while the screenshot layer can return0xe0ce.
After the crash, virtual-controller updates returned0x1c24ca; detach failed,
and attach alone did not restore input. Remote reboot was accepted, but the
server did not return within the first minute. Bootloader recovery was requested
and console ownership released to the user. This is a limitation of the current
remote-control setup, not proof that every recovery can be automated.

Evidence: private `sys-autopilot-setup/run-42bf233/`, 45018-byte final runtime
log and 130101-byte Atmosphere report; no proprietary bytes published.

## 2026-11-27: vertex-stage texture bindings

The runtime previously exposed only fragment texture binding. It now exports
sceGxmSetVertexTexture and its internal alias, snapshots descriptors separately
for both stages, and uploads a shared 32-slot descriptor table with disjoint
16-slot ranges. Each native shader stage binds its local unit to the appropriate
global image/sampler handle. Fragment setters now validate context, texture and
unit bounds. Existing texture format/layout/staging restrictions still apply.

Host regressions cover independent stages, all unit bounds, descriptor snapshot
ownership, rebinding and unchanged state on invalid calls. Actual vertex texture
sampling remains unverified on Switch; no commercial frame is claimed.

## 2026-11-27: offline GLSL-to-DKSH pipeline checks

Host UAM 1.1.0 compiles all 32 first-by-hash and 32 largest private fragment GXP
candidates after production-flag GLSL translation. Seven pinned native-tool
fixtures also compile, covering vertex and fragment stages. Reports include
translator/compiler hashes and bounded DKSH header validation. No proprietary
inputs or generated programs are committed. Controlled negative compiler checks
prove that nonzero exit or zero exit without DKSH fails the runner.

This establishes host compiler acceptance for the sampled programs, not ARM
library equivalence, GPU execution, correct rendering or commercial title boot.
Console and HLE files remain under ZCode's separate active claim; no new Codex
hardware access or deployment occurred.

## 2026-11-27: real title initialization and offline uniform-shuffle correction

The four-minute f11a064 run passed fixed-address placement and entered the real
LBP executable. Logs show guest job workers, SceGxmDisplayQueue, Render Thread,
and render-target allocations. The user observed no visuals; 32 first-use
unsupported service calls remain and no clean exit or rendered frame is proved.
Evidence remains private in `lbp-startup-3dda5cf/result-20261127-173420`.
No SD payload was changed during this read-only collection.

The shader compiler's misaligned-uniform copy path passed vector shuffle
component numbers as SPIR-V IDs. Marking those operands as literals removes the
assertion without changing component selection or disabling assertions. The
first 32 sorted private GXP candidates now translate to GLSL, including the first
eight that previously aborted. Pinned fixture reflection tests and controlled
lane-1/2/3 derivatives pass, with explicit emitted shuffle-component checks.
This is offline translation evidence only: no GPU execution or title boot proof.
The dependency source is patched into the build directory; no unpublished
submodule edit or proprietary shader data is needed to reproduce the build.

## 2026-11-27: savedata PASS, game layout retries, and offline shader failure

Owned savedata v1 passes every report marker on f11a064, including metadata replacement, reopening, truncation and cleanup. Process/launch/image-close return zero. The original game/dependencies and its real PARAM.SFO were restored and readback-verified. A subsequent game run stops at fixed-address layout retries 24/64 and 25/64, before guest entry; no game startup or rendering was exercised. The user is asked to allow the bounded retry sequence to finish.

An isolated offline translator mode passes all seven pinned shader fixtures. The first eight structurally checked private fragment programs abort on glslang's zero-ID assertion. A source-level host backtrace reaches the misaligned uniform-register copy path: vector-shuffle component literals are passed through the general createOp ID-operand API. This is a translator compatibility failure, not a game rendering result. Private corpus, dumps and backtrace remain outside the repository; no proprietary shader bytes are published. Dependency correction is pending.

## 2026-11-27: private shader-corpus discovery for LittleBigPlanet

A read-only audit found no raw GXP headers in the private executable. The FARC index contains 21,792 in-range entries, including repeated hashes. Following the public FarcTool2 container layout, 9,504 unique compressed resources decoded with bounded output and verified stored SHA-1; 66 containers remain unsupported. The decoded resources yielded 21,294 unique GXP candidates (19,273,482 bytes total) with bounded program sizes, parameter tables and parameter names. These are structural candidates, not a successful translation or rendering result.

The archive SHA-256 still matches the previously verified private payload. Original files were never modified and no game instructions were executed. The private corpus and source-bound manifest remain outside the repository and outputs; no proprietary shader bytes are published. Reference source: https://github.com/LBPModding/FarcTool2/tree/4736687e221c2d287f92538f4c6f3a413f6792b7 . The next offline investigation is semantic validation and translation of a bounded subset, with per-program failure isolation.

## 2026-11-27: owned savedata v1 reaches replacement failure on 60d7966

The report passes owned metadata identity and first save/slot creation, then fails the second DataSave with 0x80100640. The runtime explicitly returns process/launch result 1 and image-close zero. The user's clean visual closure therefore is not a successful probe pass. The second operation updates an existing metadata record, matching the non-overwrite rename failure reproduced in the host model. The new replacement fallback is not yet hardware-validated.

The active owned probe is retained for a corrected-runtime rerun. Game executable, module list and original SFO presence remain backed up on SD and locally. Evidence is private in result-5473886f8e8848d6b415ec23c1da42ab. No commercial-title rendering is established.

## 2026-11-27: owned savedata probe first attempt failed before entry

Runtime 60d7966 recognized the owned probe but automatically preloaded game libraries because the staged modules.txt was absent. Import binding then failed before guest entry, and no savedata-probe.txt was produced. This is a staging configuration error, not a savedata operation failure. The first no-dependency staging plan incorrectly treated an absent list as equivalent to an explicit empty list.

A present comment-only modules.txt was uploaded and readback-verified with the paired executable and owned SFO. Original game files remain preserved on SD and locally. No reinstall is needed for this configuration correction. The next launch must produce an actual report before any savedata hardware claim can be made. Private evidence is retained in result-6cc41603dd7447018851959e64cdae78.

## 2026-11-27: savedata call shape on runtime febb799

The new hardware log identifies runtime febb799 and records the first four argument registers. The observed SaveDataSlotGetParam call selects slot zero and a null mount pointer. SaveDataDataSave supplies one item, a non-null slot descriptor and a null mount pointer. The message-dialog initialization is followed by NetCheckDialogGetStatus with the unsupported error still in r0, consistent with the statically identified retry path. No normal exit or rendering is established by this log.

Evidence is retained privately in result-20261127-165112. The newer AppUtil storage implementation has not run on hardware. Owned savedata probe v1 is built and import-audited to validate real filesystem flush/replacement and generic savedata0 reads before a game test; no hardware pass is claimed.

## 2026-11-27: static savedata failure path after 613762b startup

Read-only inspection of the user-owned executable identifies the observed message-dialog call as system-message mode. The path selects the SDK no-space or no-memory-card message and retries MsgDialogInit on failure. The exact selected message has not been captured dynamically. The unsupported dialog return can therefore keep this path retrying without displaying its intended UI; an absent normal-exit marker does not prove a crash.

The observed AppUtil NIDs 0x93f0d89f and 0x607647ba are SaveDataSlotGetParam and SaveDataDataSave, respectively. Static call sites inspect their results and record a save failure in per-thread error storage. These calls require real slot metadata and file persistence; returning success or fabricating free space would not establish a working save system. The next storage implementation needs a title-scoped root, checked file operations, accurate capacity/error translation and transactional metadata. No such backend or storage success is claimed here.

Private source-bound static evidence is retained in startup-dialog-evidence.json. No proprietary instructions or assets are published. The febb799 hardware argument log remains pending and must distinguish the actual dynamic branch before a causal fix is claimed.

## 2026-11-27: LittleBigPlanet context and graphics initialization on e872799

The collected versioned log records sceNpTrophyCreateContext returning zero, creation of SceGxmDisplayQueue and Render Thread, and allocation of graphics buffers and render targets. The user reports automatic exit without visuals or a visible error. The collected log has no normal process-exit, launch-result or image-close marker; a clean shutdown is therefore not established. No crash report for the dedicated title was found in the exposed atmosphere/crash_reports folder.

First-use records include the unsupported sceDisplayWaitSetFrameBuf and several CommonDialog and online-service APIs. Their presence alone does not identify the cause of exit. Trophy progress, frame submission and commercial-title rendering remain unverified. Evidence was collected privately in result-20261127-155528; no proprietary assets or disassembly are included here.

## 2026-11-27: trophy initialization and guarded exit on runtime 7a39813

The versioned log shows NP_TROPHY module load returning zero, passes the earlier init failure gate, and reaches unsupported sceNpTrophyCreateContext (library 0x4332c10d, NID 0xc49fd33f). Main then exits zero. The bootstrap logs PROCESS_EXIT_WITH_LIVE_GUEST_THREADS status=0x0 native_process_exit=1 teardown_skipped=1; no subsequent guest/GPU/image teardown is claimed. This positively exercises the guard's native exit path on hardware. It establishes an unquiesced worker, not that every created worker was running.

Private static inspection identifies an explicit NPWR02031_00 communication ID and zero options at the context call. The archive was not previously staged. A new bounded reader and XML adapter validate the user-owned archive and its 84 trophies/four additional groups in a host-only context replay, without modifying files or executing guest instructions. Hardware context creation and rendering remain pending. Evidence was collected privately in result-20261127-150032; no proprietary bytes or disassembly are included here.

## 2026-11-27: LittleBigPlanet network startup on d921c27 - initialization success

The collected versioned log shows NET module load, sceNetInit and sceNetCtlInit returning zero. Repeated initialization returns EBUSY and NOT_TERMINATED respectively. Several later sysmodule requests return explicit FATAL errors. The executable creates job-manager worker records and reaches missing NP identity, AppUtil and trophy functions. It then performs libc termination and exits zero with image close zero. Created thread records do not by themselves establish that those workers ran. No rendering or game boot is verified.

Private static inspection shows AppUtilInit's return is ignored at this call site; a negative sceNpTrophyInit return takes a startup-failure branch. The next source change implements trophy-library and operation-handle lifetime, not trophy contexts, unlocks or a PSN identity. Hardware validation remains pending. Evidence was collected privately in result-20261127-144729. No proprietary bytes or disassembly are included here.

## 2026-11-27: LittleBigPlanet main entry after module-query fix ee2e65e

Installed runtime ee2e65e passed the earlier TLS-registration gate and created the executable's bringup thread. First-use records identify sceSysmoduleLoadModule, sceNetInit, sceNetCtlInit, sceNetCtlTerm and sceNetTerm, followed by libc termination handlers. The process exits zero and image close returns zero, but the user observes no visuals. This is startup progress, not a game boot. It also confirms that removing the output-size precondition in the address query moved execution past the preceding abort path.

Private static inspection shows the observed first module load requests NET (ID 1) and the network initialization supplies a 64 KiB pool with zero flags. Actual native networking, later module loads and rendering remain unverified. The kernel network/device error paths cannot be replaced by unconditional success. Evidence was collected privately in result-20261127-143443; no proprietary bytes or disassembly are included here.

## 2026-11-27: LittleBigPlanet first main-entry attempt on adbab65 - controlled abort

All seven private data/font assets were copied to app0 and readback-verified before normal main entry was enabled. User reports automatic closure after about fifteen seconds without a visible error. The collected versioned runtime log shows libc/FIOS startup success, one first-use record for SceProcessmgr/0xeb6e50bb (sceKernelCallAbortHandler) originating from libc offset 0x6fc with the Thumb bit set, then sceKernelExitProcess(1), process/launch result 1 and successful image close. No scene or rendering was observed. This positively exercises first-use logging on hardware, but is not the owned probe's complete ABI/duplicate-call validation.

Private static inspection identifies the executable startup path as TLS registration followed by abort on nonzero return. Libc's TLS registrar calls sceKernelGetModuleInfoByAddr into stack storage without initializing the output size. Our address-based implementation incorrectly required that size, unlike the pinned reference. Removing that precondition is a source-supported fix and a likely explanation of this abort; the query return and exact abort cause were not directly traced, so hardware confirmation is still required. The UID-based query retains its input-size check. No proprietary code or disassembly is included here. Evidence was collected privately in result-20261127-142426.

## 2026-11-27: repeated LittleBigPlanet library startup on tracing runtime adbab65

The collected log confirms installed runtime adbab65, compatible mapping after one layout restart, and successful libc/FIOS startup returns. It reaches LIBRARY_STARTUP_ONLY_RESULT status=0x0 main_skipped=1 close_title=1. There are no UNSUPPORTED_CALL or UNRESOLVED_TRACE_OVERFLOW records. The binding inventory still contains 395 distinct unresolved library/function pairs across the three images.

No missing-function invocation was observed in this initialization run. This is not proof of complete library correctness: the logging path itself has not yet been positively exercised on hardware. An owned ARM/Thumb missing-import probe is built and source-audited locally but has not been deployed. Main entry, Ult, game data I/O and game boot remain unverified. Raw evidence was collected privately in result-20261127-141845.

## 2026-11-27: LittleBigPlanet libc/FIOS startup on runtime 3dda5cf - PASS

User launched the private PCSF00516 dependencies-only payload and manually closed it after about one minute. Collected log identifies runtime 3dda5cf, maps LBP2_Final code at 0x81000000 and data at 0x81879000 after ten layout restarts, starts libc and FIOS in isolated module_start threads, and records LIBRARY_STARTUP_ONLY dependencies=2 startup_entries=2 main_skipped=1 followed by LIBRARY_STARTUP_ONLY_RESULT status=0x0 main_skipped=1 close_title=1. Libc requests a 0xb400000-byte SceLibcMalloc block. The copied module list and launch-mode file match the staged manifest.

This proves successful startup returns for these libraries and the real main image's required fixed mapping on this run. The diagnostic intentionally stays resident; manual closure is expected, and there is no clean process teardown claim. Unresolved function imports remain. Runtime 3dda5cf has no first-use tracer, so this log cannot establish whether initializers called those stubs. Main entry, Ult, game asset I/O, rendering and game boot remain unverified. No proprietary binaries or disassembly are included here. Raw evidence was collected privately in result-20261127-141302.

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

## Cube v8 hardware batch: 40 checks passed

Runtime verification composite ceaddb85d41d3cae39f7a9a9f8138d1570ac23d7
contains unchanged HLE source from owner073ee41 plus five Codex-owned production
paths: shader-code tail reservation, append-open log retry, display wait state
and the counted framebuffer wait export. Main guest source is ca3b556.
Exact source archive, ARM32 runtime stamp/NSO and all three NCA hash trees were
verified before installation on dedicated title0100f27432680000.

Cube v8 completed all40 PASS tags, including straight alpha, premultiplied
alpha, additive saturation, R/A-only writes and zero/two-period framebuffer
wait calls. All previous surface/gamma/depth/stencil/MSAA/texture/vertex-format
checks passed;360cube frames and intact ring guards completed. Logs capture
process exit0 and load_process_close0, followed by observed HOME/Start. Both
guests build with warnings as errors and resolve62cube/60draw source imports.
The original game executable/modules/normal mode were restored by exact
readback; no global homebrew/title override changed.

These four ordinary single-sample blend cases do not establish correctness
for every game shader, texture format, fragment-discard or render-to-texture
path. Counted-wait timing boundaries are covered by host tests; hardware proves
ABI execution and continued rendering. LittleBigPlanet still has startup
artifacts and has not reached its title menu or interactive3D. The unchanged
HLE revision requests SHUTTER_SOUND0x27, receives805a10ff and its initializer
returnsfalse. Detailed verification was returned through AGENT-PROGRESS.md;
HLE source remains with its owner. Captured GXM ColorSurfaceSetClip86456f7b
is an additional unsupported API requiring graphics-lane investigation.

## Color-store clip implementation prepared (hardware verification pending)

Real-game logs include unsupported sceGxmColorSurfaceSetClip86456f7b. The pinned
and current Vita3K references leave GetClip/SetClip unimplemented. Read-only
export decoding of the locally installed SceGxm library establishes twelve-bit
coordinate masks and optional getter outputs. Initialization packs width-1 and
height-1 as default maxima. No library code, keys or firmware binary is tracked.
Runtime descriptors retain copied clip values within the existing48-byte ABI;
BeginScene snapshots the descriptor and validates the store rectangle before
changing scene state. EndScene limits color writeback, preserving destination
pixels outside inclusive bounds without restricting depth/stencil rasterization.
Partial stores with unequal render/surface dimensions return unsupported before
scene setup; full scaling is retained. Sanitized host export/copy/mask/empty/
bounds/unsupported checks pass; cube9/draw12 compile with warnings as errors.
Seven pixel-readback cases are prepared, bringing the cube batch to47checks.
Hardware execution remains pending until the verified runtime is installed.

## Cube v9 hardware result: 48 checks passed

Frozen runtime4574c20 (unchanged owner073 HLE plus eight Codex production paths)
and corrected guestd0f375e passed all48checks. Seven color clip tests verify
unaligned inclusive bounds, a single pixel, empty rectangles,12-bit masking,
full reset, scene descriptor snapshots and region-clip intersection. An eighth
case uses forcedDF32depth readback to verify0.75 at64points inside and outside
the color clip under an explicit identityZ viewport. All earlier40checks pass,
including360cube frames/guards; log records process0 and cleanup0, HOMEobserved.
Original game/modules/normal mode restored exactly after the test.

First guest4e6ecf5 expected0.75 under defaultZoffset0.5/scale0.5 and failed its
depth check after all seven color cases passed. Default viewport maps that
position to0.875; the corrected diagnostic explicitly requestsZoffset0/scale1.
The runtime was unchanged between these two runs. Private initial/final reports
are retained. Public owned-scene report and verification receipt list all48tags.
These results establish our implementation on Switch; they do not substitute
for an on-Vita clipping oracle or prove LittleBigPlanet boots correctly.

## Release shader logging overhead

The Vita3K compatibility wrapper implemented TRACE/DEBUG as unconditional
format-and-write functions even under NDEBUG. Release now suppresses both
levels and their argument evaluation; INFO/WARN/ERROR/CRITICAL remain active.
Production-wrapper host tests explicitly force each mode and check counters
without relying on assert in Release. Both tests and existing real-GXP layout
integration pass. The original Release wrapper fails the new evaluation check.
Native/hardware performance verification of this logging change is pending;
no shader cache or precompilation feature is added. LOG_DISASM can still build
its diagnostic string for the pinned translator's optional disassembly storage,
so this change does not claim to eliminate all formatting overhead.

## Loading-frame slowdown: profiling prepared

User reports roughly one displayed update every five seconds. A50-second UDP
observer on aecf224Release captured13display-queue enqueue attempts, with median
inter-attempt gap3.2258seconds and77shader compile starts/74ends. This is a
host-receipt/UDP proxy, not an exact physical-presentation FPS measurement.
The observation confirms that draw-call counts alone did not establish smooth
animation; shader activity alone does not establish the sole bottleneck.

Native diagnostics now report per-program GLSL/UAM durations; per-scene total
elapsed time, previous-scene queue-idle wait/release, texture and uniform setup
durations, draw/batch counts and target dimensions; and timestamped framebuffer
submissions after display_state_set succeeds. These are wall times including
preemption, not exclusive CPU/GPU execution times. Scene elapsed also includes
guest work between BeginScene/EndScene. DISPLAY_SUBMIT records a ready-buffer
submission, not completion of physical scanout. Program pointer/size repetition
is a clue, not proof of identical immutable shader contents. Native/hardware
verification of these additional diagnostics is pending.


## Release profiling hardware regression

Frozen composite `0a5ea6e` combines unchanged HLE owner `491e281` with the
owned clip, logging and timing changes through `0746941`. The Release native
build and package were source/stamp verified. On Switch, visible_cube v9
passed all 48 checks, including 360 cube frames, then returned process 0 and
cleanup 0. HOME was observed and the original game executable, module list
and normal launch mode were restored by exact readback. Captured framebuffer
submission intervals have median 33,345 microseconds; this measures successful
buffer submissions, not physical scanout. All timing categories were emitted.
The earlier quiet Release `aecf224` also passed the same 48-check batch.
Release disables the SD log sink, so its old SD log is stale; these results use
fresh UDP capture and the version stamp, with best-effort delivery limitations.
The logging change did not resolve LittleBigPlanet's observed multi-second
frame gaps. Real-game timing collection is in progress; no performance fix is
claimed from the owned cube result.


## Game profiling and native stack exhaustion

A second bounded launch of frozen `0a5ea6e` entered the real game after the
first launch exhausted its 64 layout retries. Fresh UDP capture contains 755
complete shader timing records and 286 complete scene records. Median GLSL
translation was 45,269 us and the encompassing UAM phase was 517,549 us.
The UAM phase includes compilation and temporary-file output/readback/cleanup;
it does not isolate SD time. Scene retirement and texture staging also have
large wall times. These measurements include preemption and overlap across
threads, so their totals must not be added into an exclusive cost breakdown.
The trace has no repeated pointer/size/stage key; identical shader contents at
different pointers have not been ruled out. The game still displayed startup
and autosave graphics with alpha artifacts, without an interactive 3D scene.

The run ended in a native data abort around an operator HOME request; causality
between that request and the crash is not established. The crash report and
DTI2 dump agree on all crashed registers, unique thread IDs and exact EOF.
SP is 560 bytes below the native stack mapping's lower boundary. A rebuilt
`0a5ea6e` ELF's entire executable segment exactly matches the original deployed
NSO, SHA-256 `cc17955a55a76f04c144fd09ce37ef1f8e783e47770a74f3089fb9cf25908581`.
Its PC resolves to `_svfprintf_r`'s first stack store after an 816-byte frame,
with LR in `sprintf`; the format pointer addresses `%.32g`, also used by the
pinned SPIRV-Cross float formatter. This supports native stack exhaustion in
the shader translation path; a complete caller unwind was not recovered.

Guest-thread creation now reserves at least 1 MiB native backing because guest
and HLE/compiler frames share a stack, preserving larger requests. Thread-info
reports the actual native allocation. Focused ASan/UBSan tests cover small and
large requests, invalid sizes before allocation/query/create, and unchanged
thread/affinity/TLS ownership. The change increases memory per small guest
thread. Hardware verification beyond the observed failing shader is pending;
1 MiB is not a proven maximum compiler stack requirement.


## ARM32 native core-mask ABI

The first `f0f591a` owned graphics run did not enter the guest: setting native
mask 7 returned Horizon `0xe201` (InvalidCoreId), followed by process failure
and successful loader cleanup. Its previous graphics report was stale; after
removing that backed-up report, a fresh retry confirmed the same native error.
It is not a 48-check pass. The installed libnx32 header declares a 32-bit
mask, while its linked stub consists of SVC 0x0f and return, forwarding r0-r3.
Atmosphere's pinned 64From32 service consumes a 64-bit mask. A local declaration
with the correct 64-bit argument and the same assembly symbol initializes r3
instead of forwarding an unspecified upper half; the SDK remains unchanged.
Host mocks now accept the full 64-bit argument and assert zero high bits.
Those mocks do not establish the ARM register ABI or native hardware success;
compiled call-site inspection and a fresh graphics batch are still required.


## Native ABI correction hardware verification

Frozen `b8519bb` retains unchanged HLE `491e281` and eight exact owned
production paths through `4f15735`. The native ARM call site has r3=0 after
successful threadCreate and preserves that value through handle/core/low-mask
setup to SVC 0x0f. The previous report was confirmed absent before launch.
The fresh owned cube v9 run passed all 48 checks, including 360 cube frames,
returned process 0 and cleanup 0, and HOME was observed. Scene logs captured
execution on native core 1. Original game/modules/normal mode were restored
by exact readback. Median framebuffer submission gap was 33,378 us; this is
not physical scanout FPS. The real game subsequently entered, with shader
and scene records observed on cores 0, 1 and 2; it is still loading, so the
previous late stack failure has not yet been proven cleared.

The shader SDK's memory-output method was also inspected read-only with the
SDK objdump fallback, not REA decompilation (provider unavailable). It writes
through the supplied buffer unconditionally before returning the output size;
a NULL-buffer size query is not supported by this linked method. No raw memory
writer replacement was adopted. Current native phase diagnostics separate
compiler construction/CompileGlsl from file output, validation, unlink and
compiler destruction; the latter is not a pure SD-I/O measurement.


## Cached texture staging hardware verification

Frozen Release `5b97266` adds CPU-cached converted texture staging with a
checked full-allocation cache flush before publication, retaining unchanged HLE
`491e281`. The previous report was confirmed absent before launch. All 48
owned cube v9 checks passed, including texture mutations, viewport directions,
ring-buffer guards and 360 cube frames; process exit and loader cleanup were 0.
HOME was observed and original LBP/modules/normal mode were restored by exact
readback. Median framebuffer submission gap was 33,344 us, not physical scanout
FPS. This verifies pixel correctness and flush API behavior in the batch, not
a measured game speedup. The full host suite also passed, including separate
CPU/GPU cache views and flush-failure cleanup.

The preceding real-game b8519bb run progressed past the old native stack
exhaustion location, then crashed in guest rendering reading NULL+0x30 after
startup cleanup had freed a graphics-pool table. The Render Thread had a healthy
1 MiB native stack. The first failed initializer remains unidentified; no
interactive 3D boot or complete startup pass is claimed.
