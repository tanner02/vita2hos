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
