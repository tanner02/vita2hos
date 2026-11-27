Owned savedata probe v1 tests the default AppUtil storage path. It requires the
paired 64-byte param.sfo produced by build.py, identifying VHOS00001. The guest
compares every metadata byte before initializing AppUtil, refusing game metadata.
It uses only probe-save.bin and slot 0x56483253 in that owned namespace. Successful
execution removes them. Failed runs may leave those test artifacts for diagnosis.

The probe has no scene and exits automatically. Collect app0:/savedata-probe.txt
and the versioned runtime log. A hardware pass requires all these report markers:

- savedata_probe v1
- PASS owned_metadata_identity
- PASS first_save_and_slot
- PASS update_flush_and_reopen
- PASS truncate
- final PASS savedata_probe, with no FAIL lines

Also require runtime process, launch and image-close results to be zero and no
unsupported imports. Reopening occurs across AppUtil shutdown/reinitialization,
not a power cycle. This tests actual write/flush/rename behavior, metadata
replacement, offset/truncate bytes, generic savedata0 reads and mount revocation.
It does not establish power-loss durability, quota reporting or game rendering.

Build with build.py --sdk <vitasdk> --output <directory>. The output executable is
a Vita guest for vita2hos, not a Switch NRO. The builder checks its entry point,
imports and current source exports and produces a checksum manifest. No deployment
or hardware execution is performed by the builder.

Before deployment, back up the active executable, module list, launch mode and
sce_sys/param.sfo. Use this guest with its owned SFO, an empty dependency list and
normal launch mode. Restore every saved file after collecting the result. Never
install the paired owned SFO over a game without preserving the original.
