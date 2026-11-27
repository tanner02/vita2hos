The visible cube guest is version 7. It retains the existing surface, shader,
gamma, depth, stencil, texture and 360-frame animated cube checks. Its VDM,
vertex-uniform and fragment-uniform rings now share one allocation at distinct
page-aligned interior addresses. Four 4096-byte regions filled with 0xa5 surround
the rings. After rendering and cleanup, every guard byte must remain unchanged.

A successful hardware report must identify `visible_cube v7`, include
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
are surface probe v3 and draw probe v10. New versions require a runtime with MSAA
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
