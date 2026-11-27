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
