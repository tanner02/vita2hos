# Owned module-start probe

Build without executing guest code:

```sh
python3 tests/hardware/module_probe/build.py --sdk /path/to/vitasdk --output /path/to/build
```

The builder generates one owned import library, compiles both freestanding ARM
images with warnings as errors, validates the dependency exports against main's
function/variable imports, checks native import availability, and creates a
fingerprint-bound plan for two reviewed ARM/Thumb thread-pointer instructions.
The main has a 4/16-byte owned TLS template; the dependency declares no module TLS.

For a prepared hardware session, preserve the current executable, its TLS plan,
modules.txt, any existing same-named dependency, and both runtime logs. Copy all
four files under `sd/vita2hos/` together. On installed runtime 006d5c4, launch the
vita2hos HOME tile normally; no reinstall, R override, scene, or prompt is needed.
After automatic exit, collect module-probe.txt, log.txt and log.previous.txt.

Success requires final `PASS module_probe`, process exit 0 and cleanup 0. The
report checks initialized main TLS, dependency startup exactly once on a separate
thread ending via ExitThread(0), an imported function, and bidirectional shared
storage through an imported variable. The recorded child TLS address is compared
only, never dereferenced after its thread exits.

A missing report is not success: startup may have failed before main. Preserve
logs before another launch. Restore the earlier executable/plan/module list as a
matching configuration after collection. All source is owned test code; no game
assets, proprietary modules or keys are included. Passing this workload does not
establish commercial-libc initialization, dependency TLS or game compatibility.
