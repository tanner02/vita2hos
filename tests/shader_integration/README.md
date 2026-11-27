# Real shader integration checks

This native C++20 target runs the production GXP parser and shader translator,
then compares SPIR-V reflection with `gxp_uniform_layout_c`. It checks member
names/order, padded sizes, offsets and total storage-buffer size for the seven
GXP fixtures already pinned in Vita3K's native-tool directory. Controlled copies
also move a uniform into guest slots 0 and 13 to exercise named-buffer mapping.
The copies are generated in memory; the source fixtures are unchanged.
Every case must also convert successfully through the runtime's GLSL wrapper.

This validates shader declarations. It does not execute a shader, validate GPU
buffer contents, establish game compatibility, or replace Switch hardware tests.
Only logging is replaced for the native build; shader/parser code is real.

On Linux, install CMake, Ninja, a C++20 compiler and `libfmt-dev`, then run from
the repository root:

```sh
git submodule update --init external/Vita3K external/vita-headers
git -C external/Vita3K submodule update --init external/glslang external/SPIRV-Cross
cmake -S tests/shader_integration -B /tmp/vita2hos-shader-integration -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/vita2hos-shader-integration --target shader_integration -j 4
ctest --test-dir /tmp/vita2hos-shader-integration --output-on-failure
```

## Isolated corpus checks

`shader_integration --single program.gxp` bounds the input, selected tables and
parameter names, then extracts uniform layout and invokes GLSL translation with
the production runtime feature flags. It does not mutate parameters or perform
the pinned-fixture derivative/reflection checks. Vertex binding hints are absent.
These are partial structural checks, not a hardened parser for adversarial inputs.

Use `python3 tools/check-shader-corpus.py <translator> <corpus-directory>
--output <private-report.json> --limit 32 --timeout 15` to isolate each program.
The runner checkpoints after every result and records failures/timeouts rather
than treating process exit alone as success. On POSIX it disables core dumps and
limits each child to 2 GiB of address space. Keep proprietary corpora and reports
outside the repository: diagnostics may contain program details. No GPU execution,
UAM compilation or game rendering is established by a translation result.

## Misaligned secondary registers

Controlled fixture copies also move a uniform container's secondary-register
start into lanes 1, 2 and 3. Each case verifies the two emitted SPIR-V shuffle
component lists as well as uniform reflection and GLSL conversion. These copies
are translation tests; their instruction/register correspondence is not used for
rendering.

Both this target and the production shader target include
`cmake/Vita3KUniformShuffle.cmake`. It compiles a build-directory copy of the
pinned recompiler with shuffle components marked as literal operands. The original
submodule stays unchanged. Configuration fails if the expected upstream block or
source list changes, requiring review when updating Vita3K.

## Optional host UAM compilation

Add `--uam /path/to/uam` to the corpus runner to compile each translated shader
as `vert` or `frag`. The runner uses a separate temporary directory per program,
removes generated GLSL/DKSH after checking it, and records compiler diagnostics,
compiler executable SHA-256, DKSH SHA-256 and byte size in the private report.
A compile pass requires both a zero compiler exit and a DKSH file satisfying the
production wrapper's magic, offset and length checks. Translation-only runs
retain their original `translated` status; full pipeline checks use `compiled`.

The translator also accepts `--single program.gxp output.glsl` for explicit
private inspection. Generated GLSL contains proprietary program details when
using proprietary inputs. Keep it and the reports outside the repository.

This uses the selected host UAM executable. It does not execute GPU code or
prove identical behavior from the ARM compiler library, vertex binding hints,
correct pixels, or game boot. DKSH header checks are structural checks only.
