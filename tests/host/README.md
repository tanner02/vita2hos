# Host file I/O regression checks

Run on Linux or WSL with a C compiler that supports AddressSanitizer and
UndefinedBehaviorSanitizer:

```sh
git submodule update --init external/mlib external/vita-headers
sh tests/host/run.sh
```

The test compiles the production `SceLibKernel.c` module against the pinned
Vita headers and mlib dictionary implementation. Small libnx/deko3d stubs
allow it to run without the Nintendo Switch toolchain. It checks unknown,
closed, and wrong-type descriptors, file reads after seeks, absolute and
relative seek positions, and invalid seek arguments. Build artifacts are
created in a temporary directory and removed on exit.

These single-threaded checks do not validate Switch ABI details, locking,
concurrent close operations, or hardware behavior. A Switch build and console
testing are still required for those.
