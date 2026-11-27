# Hardware observations

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
