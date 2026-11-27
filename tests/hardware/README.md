# 3D hardware smoke test

Current Debug builds write `SD Card/vita2hos/log.txt` automatically. After a
test, retrieve that file and `log.previous.txt` through MTP, FTP, or an SD reader.
The current file rotates on startup and before exceeding 1 MiB; the previous
file may therefore contain an earlier run or an earlier chunk of the same run.
Copy both before running another test. Each line is closed to the filesystem;
this does not guarantee recovery after storage failure or sudden power loss.
Release builds keep file logging disabled. Console and UDP logging remain
available, including when the SD log cannot be opened.

After capturing a log from a current build, summarize missing APIs with:

```sh
python3 tools/report-imports.py game-test.log
```

The JSON report groups unresolved function and variable imports by library and
symbol NID and resolves names from the pinned Vita SDK database. Repeated log
records are counted together; those counts are not runtime call frequencies.
An empty report does not establish compatibility: partial execution or UDP loss
can omit imports. Older builds are supported through their library-header and
missing-NID messages; those entries are marked `legacy_context` because dropped
or reordered UDP messages can make that context unreliable. Structured records
are preferred when present and are not counted again through their older
companion messages. Preserve the original log with the report.

Use a Debug NSP built from the commit being tested. The upstream
[alpha-0.4 release](https://github.com/xerpi/vita2hos/releases/tag/alpha-0.4)
provides `gxm_cube_sample.zip` and `gxm_textured_cube_sample.zip` as baseline
3D applications. These are baseline checks, not evidence of commercial-title
compatibility or coverage of every new texture format/layout.

For a prepared smoke-test bundle, back up your existing runtime and executable,
then copy its `sd/` contents to the SD card. The default `executable` is the
untextured cube. The alternate textured sample is under
`samples/gxm_textured_cube_sample/executable`. `manifest.json` records the
runtime commit, source URLs and SHA-256 checksums; no hardware execution is
claimed by the bundle itself.

1. Put `vita2hos.nsp` at `sd:/vita2hos/vita2hos.nsp` and configure the 32-bit
   application override described in the repository README.
2. Extract one sample's `.velf`, copy it to `sd:/vita2hos/executable`, and remove
   its extension. Start with the untextured cube, then use the textured cube.
3. Before launching, run the log receiver on a Windows PC on the same LAN:

   ```powershell
   .\tools\capture-netlog.ps1 -OutputPath .\cube-test.log
   ```

   If the PC has several network adapters, add `-InterfaceAddress` followed by
   its LAN IPv4 address. `-SwitchAddress` can filter packets to one console.
   The script listens on multicast group `224.0.0.1`, UDP port `28771`, matching
   vita2hos. It only receives logs and appends them to the selected file.
   Network/firewall configuration can prevent multicast reception, and UDP
   packet loss means an incomplete log is possible.
4. Launch through the full application override. Record whether the sample
   reaches a visible 3D scene, whether motion continues, and whether colors,
   depth, or textures look wrong. Run each scene for at least 30 seconds.
5. Stop the receiver with Ctrl+C. Keep the log and a short video or photograph
   with the tested commit, sample name, Switch model, Atmosphere version, and
   any fatal error text. Use a new log filename for the second sample.

The current staging path uses conservative GPU waits at scene boundaries.
These tests establish a basic rendering baseline; performance, GPU cache
visibility, and swizzled/tiled layout behavior still need direct hardware
coverage. A successful cube does not prove that LittleBigPlanet boots.

Fragment-program enable controls now select front/back suppression through the
translator's helper uniforms. Disabling both effective faces masks color writes
while retaining depth/stencil processing. Two-sided mode selects separate stencil
settings without losing saved back-face state. Separate back-face depth functions
and depth-write modes remain unsupported, and draws still require a bound fragment
program. These paths need hardware coverage beyond the existing cube result.

Scene setup reloads supported linear ABGR8 color surfaces into the shadow render
target, so switching guest framebuffers does not reuse stale shadow color. The
native copy path checks dimensions, 32-byte row pitch, offset alignment, and full
image allocation bounds. Vita tiled/swizzled color surfaces require conversion
and are rejected by this path rather than treated as NVIDIA block-linear images.

Pinned sample archive SHA-256 values:

- `gxm_cube_sample.zip`: `619f2c1bbc1cdf2652025545041ac628675a18628d4fe499074cc23f71da045f`
- `gxm_textured_cube_sample.zip`: `04edb34237eec0fd030ceb897d2f810a35f071002da15ae241921296a499355b`

To create a fresh bundle from a downloaded Debug artifact:

```sh
python3 tools/package-hardware-smoke.py Build_Debug.zip --commit FULL_COMMIT_SHA \
  --build-run https://github.com/OWNER/vita2hos/actions/runs/RUN_ID --output smoke-test.zip
```

The packaging tool downloads only the two pinned sample archives, verifies
their hashes, and copies the NSP and sample files without executing them.

## FTP deployment

### Standalone HOME-menu title package

`vita2hos.nsp` from the runtime build is an ExeFS container. To create a separate
installable HOME-menu application, use `tools/package-home-menu.py`:

```sh
python3 tools/package-home-menu.py smoke-test.zip \
  --packer /path/to/hacbrewpack --nacptool /path/to/nacptool \
  --keyset /path/to/local/prod.keys --output vita2hos-home-menu.nsp
```

The default standalone program ID is `0100f27432680000`. The package contains
program, control, and content-meta NCAs, its own vita2hos name/icon, and ARM32
process metadata. It runs the runtime directly and reads `/vita2hos/executable`
and other game assets from SD. Install it through a compatible homebrew title
installer, then select the vita2hos HOME tile. No existing game or global hbmenu
configuration needs replacement. The installer and hardware launch remain
separate from packaging validation.

The first `f55edbf` standalone package installed but did not launch. Its runtime
configuration enabled two mutually exclusive debug capabilities. The corrected
configuration requests only `allow_debug`; `check-runtime-npdm.py` now rejects
incompatible generated descriptors before publication. The corrected package
still requires a successful hardware launch before this route is validated.

The packager validates the input bundle, preserves the runtime NSO, validates
content filenames against their SHA-256 digests, and records an output manifest.
Packaging keys stay local and are excluded from outputs. The default JPEG is
`assets/home-menu-icon.jpg`; `--icon` accepts another 256x256 JPEG. The external
[hacBrewPack](https://github.com/rlaphoenix/hacBrewPack) revision used for the first
package was `745b16ecfc9ce055743067d200572204cb2aac6c`.

### Automated surface/notification guest

`tests/hardware/surface_probe` builds a freestanding Vita executable with Vita SDK:

```sh
cmake -S tests/hardware/surface_probe -B build-surface-probe -G Ninja -DVITASDK=/path/to/vitasdk
cmake --build build-surface-probe
python3 tests/hardware/surface_probe/verify-imports.py --sdk /path/to/vitasdk --build-dir build-surface-probe
```

The guest imports only implemented runtime APIs and requires no libc startup or
GXP files. Package it using `package-hardware-smoke.py --surface-probe
build-surface-probe/executable` in addition to that tool's existing arguments.
Then select it with `deploy-switch.py BUNDLE --host SWITCH_IP --sample surface_probe`.
This replaces the test executable; it does not launch the console or change its
homebrew configuration.

After manual launch, read `/vita2hos/surface-probe.txt`. A final `PASS surface_probe`
means two patterned framebuffers survived A/B/A switching, notification values
completed, an invalid notification was rejected without ending the scene, and a
null-color/null-sync scene finished. A report ending at `RUNNING` or an intermediate
step is incomplete. This performs no draw calls: it does not prove shader,
depth/stencil draw behavior, or commercial-title compatibility. Retain the matching
runtime log when reporting a failure. The program exits when finished, so a visible
3D scene is not expected.

The same build also emits `build-surface-probe/draw-probe/executable`. Its
`draw_probe` variant first performs the surface tests, then renders offscreen
triangles using the pinned Vita3K `color_v.gxp` and `clear_f.gxp` fixtures. It checks
red color output, depth-only occlusion, and stencil replacement at nine interior
pixels. The vertex program uses an identity WVP matrix to preserve varying depth;
the clear vertex fixture is deliberately not used because it forces depth to one.
Version 5 then uses `texture_v.gxp` and `texture_f.gxp` to sample a 16-by-16
four-color texture with point filtering. Four interior samples verify UVs and
image layout. A second scene rotates the colors in the same guest allocation
and verifies re-upload, reporting `PASS texture_quadrants` and `PASS texture_update`.
The expected row order accounts for GXM's default negative Y viewport scale;
a third scene sets a positive Y scale and checks the reversed mapping, reporting
`PASS texture_positive_viewport`. Version 2 had an incorrect expected row order.
Version 4 also tests both reversed axes, negative X alone, and restoring the
default viewport. Each scene starts from a distinctive background, records all
four sample values, and continues after mismatches to distinguish missing
coverage from coordinate errors. A mismatch still fails the complete probe.
Version 5 first reads the initial background depth, sets a nondefault depth,
and checks both descriptor memory and the getter's returned value. The native
GXM scalar-float exports use the VFP PCS, even when the guest uses a softfp SDK:
the SDK inserts bridges before its import stubs. The runtime adapts those three
exports to its base-PCS implementations in `source/gxm_vfp.c`. SceLibm's SDK
stubs do not use these bridges and retain their existing base PCS.
The version 1 hardware pass does not establish these new texture checks.

Audit it with `verify-imports.py --variant draw_probe` in addition to the existing
SDK/build-directory arguments. Package with `--draw-probe
build-surface-probe/draw-probe/executable`, and deploy with `--sample draw_probe`.
Both probe types are checked for a runnable entry point and nonempty import range
before packaging. The draw fixture license is included as `DRAW-PROBE-COPYING.txt`;
fixtures come from the pinned GPL-2.0-or-later Vita3K native-tool sources.

The draw report is `/vita2hos/draw-probe.txt`. Its final `PASS draw_probe` line is
the completion marker. New probes call `sceKernelExitProcess` after closing the
report so the runtime can return to HOME. No visible 3D scene is expected: rendered
pixels are compared directly. A pass establishes these draw cases, not commercial
game compatibility or a complete GPU conformance result.

### Visible textured cube

The same build emits `build-surface-probe/visible-cube/executable`. Audit it with
`verify-imports.py --variant visible_cube`. It uses the runtime's GXM VFP fix
(`38972ed` or later), runs all version 5 offscreen checks, then presents a
960-by-544 rotating checker-textured cube with perspective and depth testing.
It generates its own vertices, indices and texture; no game files or additional
libraries are needed. The shaders and license are the same pinned fixtures.

Package using `--visible-cube build-surface-probe/visible-cube/executable` and
select `--sample visible_cube` when deploying. This selects the guest without
changing the bundle's default upstream cube. Confirm the expected runtime before
switching guests; the standalone HOME-menu title must already contain the fix.

The animation runs for 360 frames, logging progress every 120 frames, then holds
the last frame briefly and exits. Expect roughly 15 seconds at normal display
timing, with no interactive completion prompt. The result is
`/vita2hos/visible-cube.txt`; `PASS cube_frames` proves completion of the render
loop, and the final `PASS visible_cube` closes the report. Report whether a
rotating textured cube was actually visible: these markers alone do not prove
correct presentation or broad commercial-game compatibility.

The visible variant keeps its guest allocations until process exit so the
presenter cannot read a freed framebuffer during shutdown. This bounded test
process relies on process cleanup rather than freeing displayed memory early.

### Fixed-address loader probe

The build also emits `build-surface-probe/fixed-probe/executable`. This is an
owned `ET_SCE_EXEC` ARM executable, with code at `0x81000000` and data at
`0x81879000`, matching the address pattern observed in the selected LittleBigPlanet
build. It has no relocations or game code. Its sole import is `sceKernelExitProcess`.
The linker resolves references at their declared addresses; the generator then
encodes Vita's module-info entry format without rebasing those references.

The probe checks its actual code address, initialized data, zero-filled BSS, and
data writes. It exits immediately with status 0 on success; statuses 1/2/3/4 mean
initialized-data/BSS/write-read/code-address mismatch. Read the runtime log for
the module name `vita2hos_fixed_probe`, exact mapped addresses, guest exit status,
and successful `load_image_close()`. There is no visible scene or report file.

This requires the new fixed loader, not runtime `38972ed` or `06ded1b`. That path
requires usable CodeMemory syscalls and free permitted destination addresses.
It reserves all destinations before choosing writable aliases, rejects overlapping
or writable-executable layouts, and reports mapping conflicts without rebasing
the fixed executable. Host tests cover layout, ownership, rollback, cache calls,
and cleanup retry; hardware validation is pending. Passing this probe would not
establish TLS, bundled-module linking, or LittleBigPlanet compatibility.

The standalone runtime now checks the complete heap and IPC alias reservations,
including currently unmapped portions. On a conflict it can relaunch its own
default title (`0100f27432680000`) to obtain a different randomized layout, up to
64 restarts. A private launch packet carries the attempt count and an executable
fingerprint; changing the guest resets the count. Other host titles are excluded
from automatic restart. This is a bounded workaround, not a guarantee that a
compatible layout will be found. A test may therefore restart several times
before exiting or reporting failure. Keep the final runtime log to verify actual
mapping and execution; restart requests alone do not prove the guest booted.

Guest-library function/variable export registration is also available for startup
binding. It copies symbol tables, tracks their owning image, and rejects duplicate
providers or ambiguous symbols. A guest provider owns its entire library namespace
so missing functions cannot silently fall back to a different allocator. The
loader still needs to map/register/init the bundled SUPRX modules and bind their
variable relocations; the registry alone does not load those modules.

The export decoder accepts relocated 32-byte public-library records through a
bounded image-reader callback. It validates function targets as executable,
preserves Thumb bits, and registers all libraries atomically. TLS exports are
rejected. Library-zero lifecycle records are decoded separately into validated
start, stop, and exit function addresses, without publishing those functions as
ordinary library exports. Duplicate lifecycle functions are rejected. This is
host-tested groundwork. The runtime registers the loaded image's public exports
before binding imports, and removes them before releasing image mappings,
including cleanup failures. The default launch still supplies only the main
image; it does not yet discover or start bundled SUPRX dependencies.

The loader also exposes explicit preparation, publication, and binding phases:
`load_file_prepare` maps and relocates without resolving imports;
`load_image_publish` transfers exports into a caller-owned process registry;
`load_image_bind` resolves imports using that registry and finalizes code caches
and permissions. A process can prepare and publish every image before binding
any image, allowing dependencies to resolve each other's functions. Images must
have stable addresses, and their shared registry must outlive them. Close every
image before clearing that registry, including after a binding failure. The
existing `load_file` entry point performs these phases for one executable.
Host tests cover cross-image function binding and shared-registry cleanup; the
new multi-image API has not yet been exercised on Switch hardware.

`load_module_prepare` permits dependency modules without startup functions:
an absent lifecycle start with metadata start 0 or `0xffffffff` remains null.
An exported lifecycle start takes precedence over the metadata offset. Main
executables retain support for a valid entry at segment offset zero, and the
single-executable `load_file` rejects an image with no entry. Lifecycle decoding
does not invoke constructors or implement module-start/stop orchestration.

`load_process_load` owns a main image and an explicit list of up to 32 module
paths. It prepares all images, publishes every provider into a shared registry,
and then binds all images. Failure rolls back in reverse image order; failed
mapping cleanup retains the process owner for a later close attempt. The main
executable fingerprint survives rollback for address-layout retries. The normal
runtime launch now uses this process owner with an empty dependency list.
Automated tests inject failures into preparation, publication, binding, and
cleanup. This API does not infer module initialization order or call lifecycle
functions, and no bundled commercial module has been executed on hardware.

### Experimental title-specific launch profile

**Hardware status:** this route failed to start Mario Kart 8 Deluxe on the tested
Switch (`"Could not start the software"`), while R still opened hbmenu. The profile
was disabled. The packaging checks below validate file structure, not launch
compatibility; do not treat this route as a supported launch method.

To prepare a separate 32-bit launch route while retaining normal hbmenu,
generate an ExeFS replacement for an installed host application:

```sh
python3 tools/package-title-profile.py smoke-test.zip \
  --program-id APPLICATION_PROGRAM_ID --output title-profile.zip
```

The package contains one `sd/atmosphere/contents/PROGRAM_ID/exefs.nsp` file.
It validates the source bundle and ARM32 metadata, changes only the NPDM program
ID and allowed ID range, and preserves the runtime NSO byte-for-byte. It does
not contain or modify `override_config.ini`. Do not overlay an existing ExeFS
mod for the host application. This route is not yet hardware-verified.

With the normal `atmosphere/hbl.nsp`, R application override, and `39_bit`
homebrew configuration retained, launching the host application normally runs
vita2hos; holding R launches hbmenu. The host game's normal launch is temporarily
replaced. Rename only the new `exefs.nsp` to `exefs.vita2hos-disabled.nsp` to
restore the host game. Runtime samples/assets still belong in `/vita2hos/`.

This relies on Atmosphere's documented [ExeFS replacement and hbloader
precedence](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/loader.md).
The NSP here is an ExeFS container, not an installable game package.

Install or disable this profile over FTP:

```sh
python3 tools/deploy-title-profile.py title-profile.zip --host SWITCH_IP --port 5000
python3 tools/deploy-title-profile.py title-profile.zip --host SWITCH_IP --port 5000 --disable
```

Installation verifies normal R / 39-bit hbmenu configuration and both homebrew
files before writing. It refuses loose ExeFS mods and unknown replacements.
To update an existing profile, supply its known SHA-256 using
`--expected-installed-sha256 HASH`; the old file is backed up locally and on SD.
Uploads are downloaded and verified before activation. Disable accepts only a
replacement matching the supplied profile and renames it out of the loader path.
Neither command changes global homebrew configuration or launches software.

### Runtime and sample transfer

With the Switch FTP server open, a PC with Python 3 can transfer and verify the
bundle automatically:

```sh
python3 tools/deploy-switch.py smoke-test.zip --host SWITCH_IP --port 5000
```

Deployment transfers only the runtime and sample. It never changes Atmosphere
configuration. The former `--configure-override` option has been removed because
the global homebrew path also redirects Album, preventing access to hbmenu and
FTP. A game-specific address-space setting does not provide a separate loader
path. Keep normal hbmenu access enabled outside an explicit temporary test.

If an earlier deployment created the vita2hos override, shut down the Switch,
mount its SD card on a PC, and restore your backed-up
`atmosphere/config/override_config.ini`. If there was no original configuration,
rename the generated file to a name other than `override_config.ini` and fully
reboot Atmosphere. Preserve `hbmenu.nro` and `atmosphere/hbl.nsp`.

Existing runtime/sample files are backed up locally under `switch-backups/`
and retained on the SD with a timestamped `.backup-...` suffix. Uploads are
downloaded again for checksum verification before replacement. Add
`--sample gxm_textured_cube_sample` to select the textured cube. Transfers
require the FTP server to stay running.

For a sample comparison against the same runtime, add `--sample-only`:

```sh
python3 tools/deploy-switch.py smoke-test.zip --host SWITCH_IP --port 5000 \
  --sample gxm_textured_cube_sample --sample-only
```

This mode verifies that the installed NSP matches the bundle, then replaces
only `executable`. Atmosphere configuration is always preserved.

FTP deployment does not launch an NRO, install a title, press buttons, or reboot
the console. Exit the FTP app and use the application override manually.
