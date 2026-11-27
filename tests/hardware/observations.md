# Hardware observations

## Runtime 76a91883c32536f0db3713f05e809d90868b1060

- The user reported that the corrected standalone HOME title launched, then
  displayed the runtime's executable-loading error dialog.
- USB readback recovered `log.txt` and confirmed the guest executable matched
  the deployed SHA-256 `150f41d4ea661fa24492c973f74cfea02af271ed608898e0ecc34cb2e5992eb7`.
- The log reached JIT allocation, segment loading, and module `surface_probe`,
  then reported `Invalid module entry function.` No guest code ran.
- Inspection found the probe export configuration set `imagemodule: true`.
  Vita SDK intentionally emits no module-start entry and clears the runtime
  import range for that module type. This was a probe packaging error, not a
  missing SD file or evidence of a general loader-format incompatibility.
- The corrected `imagemodule: false` guest passed entry-point/import validation
  and was uploaded by itself. Readback SHA-256:
  `a8898d953e49835e398a1f62d0d5dd4e1e60f0d608f24f367001a6db626f9485`.
  Its hardware result is recorded below; the installed runtime was unchanged.

### Corrected surface probe: PASS

- USB readback subsequently recovered a complete `surface-probe.txt` ending
  with `PASS surface_probe`, plus the matching `76a9188` runtime log.
- The report records three successful A/B/A framebuffer pattern comparisons,
  completed notifications and a successful null-color/null-sync scene.
- The runtime log confirms module import resolution, guest execution, context
  and queue setup, color copies, and both guest/display threads returning zero.
- The app remained open because this freestanding guest returned from its
  entry rather than calling `sceKernelExitProcess`; the thread manager was
  still waiting for the process-exit event. Later probes explicitly request exit.
- This validates standalone title launch, guest loading, surface copies and
  notification handling on this Switch. The probe performs no draw calls, so
  shader, depth/stencil draw behavior and commercial games remain unverified.

### Shader/depth/stencil draw probe: PASS

- USB readback recovered `draw-probe.txt` ending with `PASS draw_probe`, and a
  matching runtime log identifying the installed `76a9188` build.
- Guest SHA-256 at deployment:
  `ec6e61ba4c77a5e72dc8c81cb6dfc168b85791bf0348b26e4547380741b3bd69`;
  source commit `820c759a734417c8a334359d8255b28ffd48b2ce`.
- All surface checks passed again, followed by `PASS shader_red`,
  `PASS depth_only_occlusion`, and `PASS stencil_replace`.
- The runtime log confirms successful Vita GXP-to-GLSL translation, UAM
  compilation, indexed draws and color readback. It ends with process exit
  status zero, runtime shutdown and a successful `jitClose()`.
- This is direct hardware evidence for the tested shader/uniform/vertex,
  depth-only, stencil and readback paths. It does not cover textures, every
  primitive/state combination, or commercial-title compatibility.

## Runtime f55edbf626acb6ff46d510a55bdd026dd78ac5ad

- Experimental route: a retargeted ARM32 ExeFS NSP at
  `atmosphere/contents/0100152000022000/exefs.nsp` for Mario Kart 8 Deluxe.
- User-reported outcome: normal launch displayed **"Could not start the software"**.
  Holding R still opened normal hbmenu.
- No `surface-probe.txt` or runtime log was found through USB/MTP after this
  attempt. There is no evidence that the runtime or guest executable started.
- Rollback: the installed file was checksum-verified and renamed to
  `exefs.nsp.disabled-f55edbf-20261127-035116`; a subsequent directory listing
  confirmed no active `exefs.nsp` remained. Global hbmenu configuration was not
  modified. Normal game launch after rollback still needs user confirmation.
- Treat this title-replacement route as failed on this setup; it is not a
  validated deployment method. Investigate a standalone installable title.
- The standalone title `0100f27432680000` installed successfully, but the user
  reported **"Could not start the software"** with no error code on launch.
  No runtime/probe report was found afterward. Installation is therefore
  demonstrated; runtime execution is not.
- Source inspection found `allow_debug=true` and `force_debug=true` in the NPDM
  configuration. Atmosphere's current `CapabilityDebugFlags::IsValid` rejects
  multiple enabled debug flags; its hbloader path explicitly rewrites them.
  This is a concrete metadata defect and a candidate explanation, not proof
  that no other startup problem exists.

## Runtime 221600951699dcad7f0592fab00efac8a30132a8

- Delivery: Debug NSP from GitHub Actions run 45900467990, packaged with the
  upstream alpha-0.4 cube samples and uploaded through FTP.
- Configuration: Atmosphere application override with `32_bit` address space.
- Untextured cube: user reported **"cube works"** after deployment.
- Evidence type: user-reported hardware result; no screenshot, video, or log
  was supplied. Continued animation, depth correctness, duration and performance
  were not separately reported.
- Textured cube: pending.
- Commercial 3D titles, including LittleBigPlanet: not tested.

This is evidence of a basic 3D boot on the user's Switch, not proof of broader
game compatibility. Keep the runtime fixed when first switching to the textured
cube so the test changes one component at a time.
