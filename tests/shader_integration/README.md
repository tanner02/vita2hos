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

## Sampler binding contract

Declaration-only copies of pinned vertex and fragment fixtures turn an existing
uniform into a sampler at each guest unit0..15. SPIR-V reflection checks the
named sampler against the shared runtime mapping: fragment0..15, vertex16..31.
The uploaded texture handle and the shader binding retain the vertex offset
when binding through deko3d, even though that call also selects a shader stage.

These copies do not add texture-sampling instructions and are never rendered.
They validate binding declarations, alongside the existing uniform/layout tests.
Actual vertex-stage sampling still needs a later hardware batch.

## Numeric vertex inputs

Metadata-only derivatives of pinned vertex fixtures check floating, integer and
raw-register input declarations against the runtime input classifier. Raw U8/S8
and U16/S16 attributes use numeric float conversion for floating shader inputs;
integer and raw-register inputs retain integer fetch. Host tests separately
cover the deko3d format mapping and stripped-shader fallback. These checks do
not establish rendered geometry; packed registers and matrix inputs still
need further compatibility work.

## Vertex input locations

For each named input in the pinned vertex fixtures, SPIR-V reflection now checks
the input's name and location against the metadata used by runtime vertex fetch.
Host checks pass attributes in a different order, include an unused attribute,
and verify sparse locations, fixed unused slots, duplicate/range rejection and
output preservation on failure. Stripped shaders retain hint-list locations.
The runtime caches the resulting native attribute state at program creation.
A stripped-symbol derivative with reversed hint order also compares its actual
SPIR-V input locations and floating types with the runtime fallback metadata.
These checks do not establish GPU fetch or rendered geometry.

## Register-format vertex inputs

Controlled vertex derivatives exercise F16, signed-byte and F32 register inputs
with array elements, plus a three-component C10 input. SPIR-V reflection checks
unsigned 32-bit input declarations, location, vector width and array length.
Each derivative must also convert through the runtime's GLSL wrapper.
Runtime metadata supplies the underlying unsigned byte/halfword/word fetch width
and total element count, including array elements and C10's packed-byte count.
Host state checks cover multi-location expansion, offsets, partial final vectors,
normalization bypass, collisions, capacity/offset limits and output preservation.

Four-component C10 derivatives with one, two and four array elements are followed
by another named input. Tests verify the packed-byte location span, adjacency,
all reflected locations and the absence of overlap. Both translator and metadata
analyzer reserve locations using the packed-byte count for register C10 inputs.
`cmake/Vita3KVertexLocations.cmake` applies this to generated build copies after
the uniform-shuffle patch, in production and integration builds. Configuration
fails if either expected upstream allocation statement or source layout changes.

Non-register array/matrix inputs remain separate work. No GPU execution or
rendered packed geometry is established by these declaration checks.
