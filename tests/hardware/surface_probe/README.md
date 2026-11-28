The visible cube guest is version 9. It retains the existing surface, shader,
gamma, depth, stencil, texture and 360-frame animated cube checks. Its VDM,
vertex-uniform and fragment-uniform rings now share one allocation at distinct
page-aligned interior addresses. Four 4096-byte regions filled with 0xa5 surround
the rings. After rendering and cleanup, every guard byte must remain unchanged.

A successful hardware report must identify `visible_cube v9`, include
`PASS cube_frames` and `PASS interior_ring_buffers_guards`, and end with
`PASS visible_cube`. A built executable or successful import audit does not
establish a hardware pass. Collect the versioned runtime log and check process
exit as well. This owned guest does not establish that a commercial game boots.

Build with the existing CMake project and VitaSDK; the target is
`visible_cube_guest`. `verify-imports.py --sdk <sdk> --build-dir <build>
--variant visible_cube` checks its entry point, import range and runtime exports.
All three guests also run 2x and 4x multisampled color load/resolve preservation
and a return to1x before the draw/cube checks. Each test switches guest buffers
and verifies every pixel in both, including the inactive buffer. Require
`PASS multisample_2x_preservation`, `PASS multisample_4x_preservation` and
`PASS multisample_restore_1x`. These cases do not draw and do not prove sample
coverage, sample positions or antialiased geometry. The other target versions
are surface probe v3 and draw probe v12. New versions require a runtime with MSAA
attachment/resolve support; do not infer a hardware pass from compilation.

The draw and visible-cube variants also draw a near green triangle followed by
a far red fullscreen triangle at each2x/4x/1x mode. Each mode uses a matching
fragment-program variant and render target, with transient depth and no forced
guest depth-plane transfer. Interior/exterior samples check occlusion. Eight
pixels on an oblique edge must contain both red and green after MSAA resolve;
the restored1x pass must contain only a solid red or green at those pixels.
Require `PASS msaa_2x_coverage_depth_resolve`,
`PASS msaa_4x_coverage_depth_resolve` and
`PASS msaa_restore_1x_coverage_depth`. These report lines establish the tested
draw only when observed on hardware. They do not prove exact Vita sample
positions, gamma-correct resolve or commercial-title compatibility.

Do not replace an active game-test payload until its report has been collected.
The guest is a Vita executable for vita2hos, not a directly launchable Switch NRO.

The draw and cube guests also draw a fullscreen textured quad using ten UV
attribute formats: U8, S8, U16, S16, U8N, S8N, U16N, S16N, F16 and F32.
Position and UV use separate streams; each packed vec2 has a nonzero offset and
poison padding. Raw unsigned endpoints are 0/1, signed endpoints are -1/1,
normalized endpoints encode 0/1 or -1/1, and floating inputs encode -1/1.
Four interior samples check the expected clamped texture quadrants. Each case
rotates the palette and starts with a distinctive untouched framebuffer value.
Require all ten `PASS vertex_fetch_<format>` lines plus the existing final cube
and guard passes. These checks cover numeric conversion, signedness, offsets
and stream binding for vec2 inputs; they do not establish packed register,
UNTYPED, vertex-array or matrix input behavior.

Version 8 adds four framebuffer readback checks over a known CPU-written
background: straight alpha, premultiplied alpha, additive blending and R/A-only
writes. Independent alpha factors and distinct background channels distinguish
missing blending from alpha-factor or channel-mask errors. Interior samples
allow two UNORM rounding steps. The first cube frame also calls counted
framebuffer waits for zero and two periods; host tests establish target timing,
while this hardware check establishes import/ABI execution and continued frames.
A complete cube v9 run has 48 PASS tags. These additions require a runtime with
sceDisplayWaitSetFrameBufMulti (1e9f698 or later); the installed abc5a95 lacks it.

Version 9 adds eight color-store clipping cases: unaligned inclusive bounds,
one pixel, an empty rectangle, twelve-bit coordinate masking, full restoration,
BeginScene descriptor snapshot and intersection with tiled region clipping, and depth writes outside the color clip.
Each case checks every pixel of a 64x64 target, including preserved destination
pixels. This is output clipping; rasterizer/depth clipping remains independent.
Requires the color-clip runtime exports and clipped store path, not just 1e9f698.
Partial clipped stores with unequal target/surface dimensions are explicitly
unsupported; full-surface copies retain the existing scaling behavior.


## Optional texture usage batch

Configure with `-DVITA2HOS_TEXTURE_USAGE_PROBE=ON` and build
`visible_cube_guest` to produce visible cube v10 (draw probe v13). The default
option remains OFF, retaining visible cube v9 and draw probe v12. The optional
batch requires the shader-specific texture-usage runtime.

The batch copies all four pinned shader fixtures into one tracked guest
allocation, so runtime metadata extraction can narrow masks rather than taking
the unknown/static-span fallback. It binds unmapped address0x1000 in unused
fragment unit15 and vertex unit15 (descriptor slots15/31). A successful solid
red draw emits `PASS inactive_texture_bindings_15_31`. Those bindings remain
present throughout the remaining cases; neither shader uses them.

In the first texture scene, it binds texture0 while a flat fragment shader is
active and draws. It then activates the sampling fragment shader and draws
again without rebinding the texture or starting another scene. Expected texture
quadrants must survive this descriptor transition; require
`PASS texture_shader_switch_without_rebind`. This validates fragment activation;
vertex coverage here is inactive slot31, not activation of a vertex sampler.

A hardware pass requires a fresh `visible_cube v10` report with all50 checks,
the two new PASS tags, all prior pixel/guard/cube checks, and runtime process0
and cleanup0. Runtime `GXM_TEXTURE_USAGE` logs must show bounded extraction
(mask0 for the vertex and flat-fragment fixtures, mask1 for the texture fragment).
Compilation, import audit and source review alone establish no hardware pass.


## Optional small mip-chain batch

Also enable `-DVITA2HOS_TEXTURE_MIP_PROBE=ON` (requires texture-usage mode).
Visible cube v11/draw probe v14 add four owned-pixel cases after the existing
checks. The expected successful visible-cube report contains54checks. RGBA8
uses a64x8 base, whileBC1/BC2/BC3 use64x32. Each texture has two levels: solid
red base and solid green mip1. The64pixel viewport selectsLOD0; nine interior
samples must all remain red. Pixel failures accumulate across all four formats.
Require PASS texture_mip_rgba8, texture_mip_bc1, texture_mip_bc2 and
texture_mip_bc3, plus all prior tags and clean runtime exit/cleanup. This batch
exercises native tiled mip offsets and staging; build success alone proves no
pixel result. A baseline mismatch is evidence to retain, not a graphics pass.

## Optional texture alpha batch

Enable `-DVITA2HOS_TEXTURE_USAGE_PROBE=ON`, `-DVITA2HOS_TEXTURE_MIP_PROBE=ON`
and `-DVITA2HOS_TEXTURE_ALPHA_PROBE=ON`. Alpha mode requires mip mode and runs only
after successful prior checks. It produces visible cube v12/draw probe v15;
all defaults remain unchanged. A successful visible-cube report has **80 PASS
tags** (54 prior plus 26 alpha cases); draw probe v15 has **77 PASS tags**.

Require both `PASS texture_alpha_<case>_raw` and
`PASS texture_alpha_<case>_blend` for each of these 13 case names:
`rgba8_zero`, `rgba8_partial`, `rgba8_full`, `bc1_opaque`, `bc1_transparent`,
`bc1_1bgr_opaque`, `bc2_zero`, `bc2_partial`, `bc2_full`,
`bc3_eight_interpolated`, `bc3_six_interpolated`, `bc3_six_zero`,
`bc3_six_full`. BC1 transparent uses color0<=color1/selector3; the 1BGR
control requires opaque black. BC2 alpha nibbles are 0/8/15. BC3 tests
240>16 with selector3 (alpha176), and 0<=255 with selector4 (alpha153),
selector6 (zero) and selector7 (full).

One additional tracked 4096-byte texture allocation is reused for all formats;
existing tracked texture shaders and vertex/index storage are reused. An
additional straight-alpha fragment-program variant is released on success or
failure. Each case samples nine interior pixels, first without blending and
then over CPU background ABGR `0x80604020`. RGB factors are source alpha and
one-minus-source-alpha; alpha factors are one and one-minus-source-alpha.
Expected values use independent integer codec/blend arithmetic. Raw samples
must match exactly; blending permits one UNORM8 rounding step per channel.
Pixel failures accumulate across all formats; API errors stop the batch.

This is positive coverage of uniform nearest-sampled alpha blocks, not a
presumed fix for game artifacts. It does not cover varying block selectors,
filtered edges, premultiplied texture data or commercial assets. Compilation
and import audits establish no hardware pixel result; require all prior tags,
all 26 new tags, runtime process0/cleanup0 and a fresh versioned report.
