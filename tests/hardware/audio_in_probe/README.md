# Silent microphone probe

Freestanding owned Vita guest for the silent AudioIn provider. All four VOICE/RAW
configurations check guarded silence, null pointers, single-port ownership,
release and closed reads. Four captures per configuration measure at least three
sample periods without report I/O between calls (two-second upper bound).

Expect 32 PASS lines, including `batch_complete`, in `audio-in-probe-v1.txt`,
plus runtime process exit 0 and cleanup 0. An incomplete report is not a pass.
This verifies silent input and native pacing; it does not establish real microphone
capture or game startup progress. Host tests separately cover deterministic
close/reopen interleavings.

Build without installing or launching:

```sh
python3 tests/hardware/audio_in_probe/build.py --sdk /work/vitasdk-hardware/vitasdk --output /work/audio-in-probe
```
