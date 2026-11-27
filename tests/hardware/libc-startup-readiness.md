# Commercial library startup readiness

Source baseline: 65ee83d (2026-11-27). Owned dependency startup, main TLS,
function imports and writable variable aliases passed on runtime 006d5c4.
Owned rendering/gamma/cube probes passed on 4d1233b. Commercial initialization
and game execution have not been verified on hardware.

## Structural binding replay

A host replay uses the production relocation, publication, binding and TP-plan
code on the user-owned main/libc/FIOS/Ult images. Source-available native functions
use synthetic host addresses, never executed. Native variable addresses are
also simulated. This checks table wiring, not HLE semantics or ARM execution.
All four images bind successfully; unresolved functions become error stubs:

| Image | Remaining function error stubs |
| --- | ---: |
| LBP2_Final | 372 |
| SceLibc | 2 |
| SceLibFios2 | 23 |
| SceUlt | 11 |

The main process parameter address is 0x8187a3b4, its libc parameter address is
0x817c5cdc, priority is 64, stack is 0x40000, and main TLS is 56 initialized / 128
total bytes. Fixed main mapping at 0x81000000 still needs hardware verification
for this actual image. The reviewed libc TP plan applies one Thumb site and
rejects a stale fingerprint without writes. No main/FIOS/Ult TP byte candidates
were found in the earlier inventory; byte scans do not prove control-flow coverage.

The variable (library 0x5ad9c136, NID 0xdf084dfa) is `__sce_libcparam`.
`load_process_parameters_register` validates the main's mapped parameter block
and publishes its guest libc address. An audit of EXPORT macros alone misses
that loader-provided variable. Actual image binding succeeds with this variable.

Libc's remaining function imports are SceLibKernel/0x15e2a45d
`sceKernelCallModuleExit` and SceProcessmgr/0xeb6e50bb
`sceKernelCallAbortHandler`. The pinned Vita3K implementations are unimplemented.
Private static scanning found direct call candidates in termination-related
contexts; no ARM code was executed and the full contracts remain unknown.
Returning success blindly would hide unimplemented behavior. The runtime's
existing unresolved-function stubs return NO_FUNC_NID, so source availability
alone cannot establish whether an initializer will reach such a stub. Missing
variables, in contrast, fail binding. Successful startup returns cannot prove
that an initializer handled every unsupported call correctly.

## Controlled startup diagnostic

`/vita2hos/launch-mode.txt` containing `dependencies-only` enables a developer
check that starts configured dependencies in order and never calls main. The
bootstrap logs `LIBRARY_STARTUP_ONLY_RESULT` with its status and remains resident
until manual title closure, for both success and failure. This retains mappings
and native objects: module-start joins do not prove resident workers stopped.
A zero status establishes successful startup returns only, not full correctness.
Close the title manually and reconnect USB to collect logs. Set the file to
`normal` or remove it before ordinary probes or game entry. Absence means normal;
malformed files fail before guest loading. The first private diagnostic payload
is now deployed; execution results remain pending. No proprietary code, assets,
disassembly or keys are stored in this document.

## First preload phase deployed; hardware result pending

The first private payload retains the unmodified audited main, libc and FIOS
plaintext images, the reviewed source-bound libc TP sidecar, an explicit
libc-then-FIOS module list, and dependencies-only mode. Production parsers accept
those actual option files; host structural replay binds all three images with
372/2/23 remaining function error stubs respectively. No ARM execution occurred.
A two-image main/libc trial instead leaves thirteen libc error stubs: eleven
libc function imports are provided by FIOS. That incomplete graph was rejected.
Ult is deferred from this normal-preload phase.

Skipping main entry does not prevent initializer-driven executable callbacks or
constructors from running. Before activating this payload, verify the new
startup-capable runtime with an owned kernel probe and its versioned log; an
older runtime would ignore launch-mode.txt. The owned kernel probe has now passed on installed runtime 3dda5cf, including
clean guest/process/launch/close returns. Its versioned log establishes that
the installed runtime understands dependencies-only mode.

The private main/libc/FIOS payload, reviewed libc TLS plan, module list and
launch-mode file were then copied over USB/MTP and individually readback-checked
against the private manifest. The previous kernel executable is preserved on
the SD card under a timestamped backup filename. The game main executable was
activated last, after its dependencies and launch mode were verified. No game
assets were copied. This records deployment only: commercial initialization,
actual fixed-address mapping and a game boot remain unverified.

Source commit 9a7eb87 adds bounded first-use diagnostics for missing functions
and separates those records from the binding inventory in report-imports.py.
Host sanitizer tests and ARM builds pass, but this tracer has not been run on
hardware and is not in the installed 3dda5cf runtime. The pending first startup
run therefore provides binding/startup logs, without first-use call evidence.
