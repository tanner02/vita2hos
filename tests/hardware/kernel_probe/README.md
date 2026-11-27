# Owned kernel compatibility probe

Build with `python3 build.py --sdk /path/to/vitasdk --output /path/to/build`.
The builder uses freestanding ARM code with no libc startup, checks the final
guest format, and audits every import against runtime source exports. It never
runs or deploys the executable. No game code/assets or keys are involved.

The probe writes `/vita2hos/kernel-probe.txt` and exits automatically. It has no
scene or confirmation prompt. Required checks are retained/duplicate/timeout
signals, caller-pool mspace bounds/exhaustion, 128 thread signal/allocation round
trips, and named condition release/reacquisition. Success requires final
`PASS kernel_probe`, exit status zero and a clean runtime teardown log.

Install a runtime containing commit 281582e or later before deployment. Runtime
4d1233b lacks the heap/signal APIs: do not run this probe with that installation.
Back up the active executable with any modules.txt / executable.tlsplan, and
remove those sidecars from the active filenames for this standalone guest.
No executable.tlsplan is needed: this probe has no guest TP instructions or TLS.
Use the existing dedicated HOME title, preserving global hbmenu configuration.

On worker failure the main thread attempts a bounded join before releasing
shared objects. If joining cannot be verified, the probe records that failure
and remains idle; close the title manually before reconnecting USB. Do not
interpret an absent final result or a timeout report as a pass.

Coverage does not include condition deletion races, signal callback waits,
nonzero delay timing, heap realloc/alignment/statistics, Ult fibers, commercial
library startup, or a game boot. Those need separate evidence.

If a newer runtime has launch-mode.txt from a library diagnostic, remove that
file or set it to normal before running this probe; dependencies-only would
intentionally skip its module_start entry and produce no new kernel report.
