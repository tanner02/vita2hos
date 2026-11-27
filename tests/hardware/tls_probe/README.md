# Owned TLS hardware probe

This freestanding guest tests the experimental source-bound TLS path without
loading commercial code or shaders. It performs no rendering and ordinarily
returns to HOME automatically. Hardware execution has not yet been performed.

Build it with the pinned Vita SDK:

```sh
python3 tests/hardware/tls_probe/build.py --sdk /path/to/vitasdk --output /path/to/tls-probe
```

The builder checks the final guest entry/import table, all ten runtime import
providers, both named ARM/Thumb instruction sites, and the 4-byte initialization
template with 16-byte total TLS size. It explicitly sets the owned guest's Vita
TLS descriptor after conversion because the pinned converter leaves those fields
zero. It generates `executable`, `executable.tlsplan`, and `manifest.json` with
checksums and site locations. The source image for any supplied game is untouched.

The probe needs runtime commit `88ed7d9` or later and its matching sidecar beside
`/vita2hos/executable`. An absent sidecar fails the pointer-ownership check before
any read/write through the returned pointer. Use no dependency module list for
this self-contained guest. Merely building or packaging the probe does not install
it or change the Switch.

Future hardware verification should retain `/vita2hos/tls-probe.txt` and the
runtime log. A passing run reports `PASS main_tls`, `PASS worker_tls`,
`PASS thread_isolation`, and `PASS tls_probe`, with guest exit code 0 and clean
runtime teardown. The main and worker each perform 32 native delay/yield calls;
their ARM and Thumb reads must continue returning their own initialized TLS area.
The worker returns normally and the main checks its pointer and private marker
after joining it. Report I/O failure produces exit code 26.

If a worker join cannot be verified, the probe records a failure and parks rather
than requesting teardown with a potentially live worker. Close the title in that
case. Other normal test failures return to HOME with a nonzero result. No prompt
or visible scene is expected.

The host model exercises the actual guest C source with mocked SDK calls,
including the passing path, an invalid thread-pointer value, worker initialization
failure, and report write failure. It does not verify Horizon registers or context
switch behavior; only an eventual hardware run can establish those results.
