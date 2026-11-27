# Owned real-libc TLS probe

This freestanding ARM guest calls the real guest libc's `__tls_get_addr` import
(library `0xbe43bb07`, NID `0xbc529b7b`). It checks the initialized TLS word,
adjacent zero-initialized storage, stable addresses across 32 delay calls per
thread, and independent main/worker writes. The main verifies its marker after
joining the worker. It performs no rendering and does not contain game code.

Build from the repository root on Linux:

```sh
python3 tests/hardware/libc_tls_probe/build.py --sdk /path/to/vitasdk --output /path/to/libc-tls-probe
```

The builder validates all ten imports, including the single external libc
provider, and publishes an owned 4-byte initialization template with a 16-byte
TLS area in the converted module metadata. It emits `executable`, an explicit
libc-only `modules.txt`, and a checksum/import manifest. It does not deploy or
execute anything. A native build validates structure, not TLS behavior.

The test needs a user-supplied matching `sce_module/libc.suprx` and the reviewed,
source-bound `libc.suprx.tlsplan` already used to load that library. Neither is
bundled. Preserve the active executable and module list before replacing them;
do not run with automatic dependency discovery or FIOS preloads. The owned main
has no thread-pointer instructions, so its `executable.tlsplan` must be absent.
The installed runtime must implement module metadata/TLS queries and dependency
startup. No SFO or game data modification is needed.

Launch the existing vita2hos HOME title normally. It should return automatically
and write `/vita2hos/libc-tls-probe.txt`; there is no scene or prompt. PASS needs
all four markers (`main_libc_tls`, `worker_libc_tls`, `libc_thread_isolation`,
`libc_tls_probe`), guest exit zero, no unsupported calls, and clean teardown.
Call/address records help identify an incomplete or crashing run. A failed join
records failure and parks; close the title instead of assuming workers stopped.
Restore the preserved game executable/module list after collecting results.

A pass isolates this library path under this probe's initialization sequence.
It does not prove the game's thread-local object was initialized, identify its
crash's root cause, or establish rendering/game compatibility. No hardware pass
is claimed from building the probe.
