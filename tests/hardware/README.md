# 3D hardware smoke test

After capturing a log from a current build, summarize missing APIs with:

```sh
python3 tools/report-imports.py game-test.log
```

The JSON report groups unresolved function and variable imports by library and
symbol NID and resolves names from the pinned Vita SDK database. Repeated log
records are counted together; those counts are not runtime call frequencies.
An empty report does not establish compatibility: partial execution or UDP loss
can omit imports. Builds before the structured `UNRESOLVED_IMPORT` records are
not supported by this reporting tool. Preserve the original log with the report.

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
