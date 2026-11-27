The visible cube guest is version 4. It retains the existing surface, shader,
gamma, depth, stencil, texture and 360-frame animated cube checks. Its VDM,
vertex-uniform and fragment-uniform rings now share one allocation at distinct
page-aligned interior addresses. Four 4096-byte regions filled with 0xa5 surround
the rings. After rendering and cleanup, every guard byte must remain unchanged.

A successful hardware report must identify `visible_cube v4`, include
`PASS cube_frames` and `PASS interior_ring_buffers_guards`, and end with
`PASS visible_cube`. A built executable or successful import audit does not
establish a hardware pass. Collect the versioned runtime log and check process
exit as well. This owned guest does not establish that a commercial game boots.

Build with the existing CMake project and VitaSDK; the target is
`visible_cube_guest`. `verify-imports.py --sdk <sdk> --build-dir <build>
--variant visible_cube` checks its entry point, import range and runtime exports.
The other targets retain surface probe v2 and draw probe v7 behavior.

Do not replace an active game-test payload until its report has been collected.
The guest is a Vita executable for vita2hos, not a directly launchable Switch NRO.
