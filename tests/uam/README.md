# UAM indirect-load clone regression

`clone.cpp` is an original IR-only fixture. It constructs a global load with
two address dimensions and checks that a shallow instruction clone retains
both indices, their address values, and `usedAsPtr` on each source. It uses
explicit result checks even when UAM is built with `NDEBUG`.

Against a completed Linux Meson/Ninja UAM build:

```sh
python3 tests/uam/run.py --build /path/to/uam-build
```

The runner reuses the build's actual compiler and link commands, replacing
only the CLI main object. Temporary test files are removed afterward; it
does not modify the UAM source, build objects, or installed SDK. Compiler
launchers and configured sanitizer/LTO link flags are retained.

The reference source at commit `5a5afc2bae8b55409ab36ba45be63fcb73f68993`
fails with exit status 1. Apply the standalone patch to an isolated source
copy, rebuild it with Meson, and repeat the test; it must print PASS and
exit 0. The runtime applies the same fix to its SDK source and archive in
the CMake build directory. The separate host wrapper test checks that normal
optimization level 3 is requested.

A private shader captured from the prior Pipeline compiler fault reproduces
the crash at optimization levels 2 and 3. Disabling MemoryOpt alone or
preserving clone metadata allows it to compile. Three patched level-3 replay
runs also succeed. That game-derived input is intentionally not distributed.
This fixture tests cloning, not GPU execution or game startup.

## Live-range interval index

The runtime also rebuilds the SDK interval utility object with a synchronized
side index for overlap queries. It preserves the `Interval` class layout and
invalidates stored ranges on extend, clear, destruction and the consumed side
of unify. The preparer checks the complete normalized source/header hashes so
SDK changes require a review of every mutation path. The installed SDK is not
modified. The compiler wrapper still serializes UAM's other global state.

```sh
python3 tools/prepare-uam-intervals.py --source /path/to/uam/mesa-imported/codegen/nv50_ir_util.cpp --output /tmp/uam-index.cpp
python3 tests/uam/run.py --build /path/to/uam-build --fixture intervals --interval-source /tmp/uam-index.cpp --sanitize
```

The original interval fixture uses a separately coalesced range-vector oracle
for 700,000 seeded mutations, overlap and containment queries, copies, clearing,
unification and four concurrent workers. The indexed variant additionally checks
that every cache entry retires after interval destruction. The runner validates
the generated implementation and rejection of changed SDK source/header input.
Omit `--interval-source` to run the same semantic fixture against the original
implementation. Sanitizers instrument the fixture and included utility source;
the remaining existing UAM objects retain their original build flags.

Private shader replay timing and output-byte comparisons are separate from this
semantic test. Host speed improvements do not prove Switch compilation speed or
GPU correctness; those require native validation.
