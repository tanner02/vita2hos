# Host regression checks

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
relative seek positions, and invalid seek arguments. Utility checks cover
binary and text round trips, empty and missing files, non-seekable input,
oversized sparse files, and write errors (including buffered errors on close).
The utility tests use Linux `/proc` and `/dev/full`. Build artifacts are
created in a temporary directory and removed on exit.

Module checks exercise function lookup and require pointer-bearing export
sections to remain writable in non-PIC objects, as required by Switch loader
relocation. The runner uses `readelf` from binutils for this check.

GXM checks exercise the production uniform-upload function, covering F32 and
F16 scalar/vector arrays, partial updates, array padding, invalid parameters,
and binary16 rounding and special values. The host compiler's `_Float16`
conversion provides an independent reference for 100,000 binary32 samples.
Integer and packed C10 uniform conversion remain unsupported.

SceLibc checks resolve the production library through its NIDs and exercise
allocation/reallocation/free, overlapping moves, numeric parsing and a sorting
callback. These host tests cannot prove the ARM32 ABI; the Switch build remains
the compile/link check, and console runs are needed to verify execution.

SceSysmem checks cover requested alignment, zero-filled memory, CDRAM alignment,
invalid and freed UIDs, interior/end address lookup, malformed options, mapping
failure cleanup, and 1,000 allocation/free cycles. GPU mapping is simulated;
these checks do not establish real GPU visibility or cache behavior.

SceLibm checks call float and double exports by NID, including trigonometry,
roots, powers, output-pointer arguments, mixed float/integer arguments, signed
zero, infinities and NaNs. They use the host math library; target newlib's
numerical edge behavior and the Vita calling convention need console checks.

Texture-state checks exercise linear descriptor initialization, zero-dimension
rejection, point/linear filter changes, supported address modes and safe aliases,
data-pointer updates, null pointers, and preservation of unrelated descriptor
bits. Mipmapped filters and border-ignore modes remain unsupported.

Texture-layout checks cover format/channel translation for uncompressed
normalized and floating-point textures, eight-pixel source row padding,
explicit stride decoding, and rejection of unsupported layouts without changing
the output. Compressed, packed, gamma-corrected and cube textures still need
support. Host checks cannot establish GPU sampling results.

Texture-staging checks verify rectangular Morton layouts against independent
recursive fixtures, non-power-of-two swizzled images, 32x32 tiled images,
1/2/4/8-byte texels, short row repacking, padding, allocation failures, and
explicit lifetime retirement. The renderer retains staging blocks through the
scene and waits for the GPU before releasing them at the next scene or context
destruction. This conservative synchronization can stall rendering; performance
and hardware cache visibility remain unverified.

These single-threaded checks do not validate Switch ABI details, locking,
concurrent close operations, or hardware behavior. A Switch build and console
testing are still required for those.
