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
