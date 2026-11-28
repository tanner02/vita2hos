# Owned NGS runtime probe

Freestanding Vita `module_start` batch, using the public native32 declarations in
`include/modules/SceNgs.h`. It does not use Vita libc startup, create threads,
produce audio, or draw a frame. Every report line is separately opened, written
and closed at `app0:/ngs-probe-v2.txt`; each run truncates the report and writes
`NGS_PROBE version=2`. Expect49 PASS tags including `guards` and `batch_complete`, plus runtime process0
and cleanup0. `batch_complete` and exit status0 mean all checks passed.
On failure, prerequisite-dependent checks stop and cleanup is best effort;
exit status is 1. An absent or incomplete report is not a pass.

Coverage includes sysmodule 0x0b, distinct voice definitions, required-size queries,
caller-owned system and rack buffers, stable and distinct voice handles, voice
metadata, EQ lock interface 0x01015cec with a write at data+0x54, persistence after
unlock/relock, isolation between voices, silent MasterOutput state (128*4 bytes),
24-byte Simple Player state, bypass, a valid 80-byte EQ parameter record plus
invalid null-record rejection, two-argument routing
creation and volume matrix, play/pause/resume/kill state, update, release and
stale-handle rejection. All caller data/output structs and three aligned 64-KiB
backing buffers have canaries, checked after cleanup. Memory-size queries must fit
these static buffers before initialization proceeds.

Version 2 also opens independent MAIN/BGM stereo AudioOut ports at 128 frames and
48 kHz using a guarded, static zero-PCM buffer. It times both initial submissions,
the next MAIN and BGM submissions, and NULL flushes using
`sceKernelGetProcessTimeWide`, with no logging between these calls. Eight audio
tags cover open, submissions, flush, a total interval of 4 ms to 1 second after
initial queuing, release, and stale output rejection. Timing details are reported
in microseconds. Individual calls have a generous one-second ceiling; reported
BGM versus MAIN timings expose extra waiting without a fragile submillisecond
assertion. Exact deadline independence is covered by the runtime fake-clock host
tests, rather than inferred conclusively from a scheduler-dependent hardware run.
This probe does not sleep arbitrarily or produce nonzero PCM.

From WSL, using the existing mounted VitaSDK:

```sh
python3 tests/hardware/ngs_probe/build.py \
  --sdk /work/vitasdk-hardware/vitasdk \
  --output /mnt/c/Users/Tanner/Documents/Codex/work/ngs-runtime-verification/probe-build
```

The build uses `-Wall -Wextra -Werror -nostdlib` and `SceNgsUser_stub` (the SDK's
`SceNgs_stub` contains the separate Internal API), validates the generated Vita ELF,
and audits every import against current runtime library/NID exports. It requires
exactly the 25 public NGS imports in library 0xB01598D9 and writes a hashed manifest.
It never installs or launches anything. Successful compilation is not hardware
verification or proof of audio emulation; the current implementation is silent.
