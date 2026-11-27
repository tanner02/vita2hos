# Owned missing-import tracer probe

Build with `python3 build.py --sdk /path/to/vitasdk --output /path/to/build`.
The builder compiles freestanding ARM and Thumb callers, creates two deliberately
unprovided imports in library 0x6f776e74, audits every other import against source,
and validates the resulting guest. It never executes or deploys the probe.

Each caller invokes its own import 64 times and checks NO_FUNC_NID in r0 and
an unchanged 0x12345678 in r1. Both assembly wrappers preserve eight-byte stack
alignment. The Thumb call crosses into an ARM import stub and returns to Thumb.
The probe writes /vita2hos/missing-import-probe.txt and exits automatically.

Requires a runtime containing 9a7eb87 or later. Old error stubs can pass the
register checks, so a PASS report alone is insufficient: the runtime log must
contain exactly two first-use records, one per owned import, with nonzero stub
and caller addresses. Check the report and complete single-run log using:

```
python3 verify.py missing-import-probe.txt log.txt --runtime COMMIT
```

Before a future deployment, preserve the active executable and its module/TLS
sidecars. This is a standalone guest: modules.txt and executable.tlsplan must
be absent, and launch-mode.txt must be absent or normal. Use the dedicated HOME
title and preserve global hbmenu configuration. No probe has been deployed yet;
the private LittleBigPlanet library diagnostic remains active on the Switch.

This does not verify floating/aggregate unsupported return conventions, actual
missing API implementations, trace-pool exhaustion on hardware, or a game boot.
It contains no proprietary code or assets.
