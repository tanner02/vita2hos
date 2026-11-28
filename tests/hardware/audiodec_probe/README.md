# Owned Audiodec runtime probe

Freestanding native32 Vita `module_start`, using the pinned SDK's
`psp2/audiodec.h` declarations directly. There is no Vita libc startup, thread,
audio playback, graphics, or commercial content. It uses all eleven corrected
runtime APIs in native library `SceAudiodec` NID `0x2AA62046`; the SDK's additional
`DecodeNStreams` API is outside this runtime batch.

Each run truncates `app0:/audiodec-probe-v2.txt`, writes
`AUDIODEC_PROBE version=2`, and separately opens/writes/closes each result line.
The complete successful batch has43PASS tags, including guards/batch_complete.
`batch_complete` and exit status0 indicate all checks passed; failures stop
dependent checks, perform best-effort cleanup, and exit1. A missing, stale, or
incomplete report is not a pass.

The batch initializes an AT9 library with a two-channel budget, queries context
size, creates an internal decoder, checks published codec/control metadata,
decodes one owned frame and then two after ClearContext, and checks internal
error0. It verifies consumed/produced counts39/512 and78/1024 and all-zero PCM.
PCM starts as A5 bytes, so these checks require actual output writes; unused PCM
after the single-frame result must remain A5. It also checks invalid null/shape/
configuration/zero-frame calls leave PCM untouched and preserve guards. A valid
control with zero requested frames reports zero bytes consumed/produced;
malformed control validation preserves its existing output fields. Argument
validation errors must not create a decoder-internal error.

A second decoder uses a guarded1-MiB static context with a256-byte guard prefix
and256-byte aligned backing address. The queried size must fit before creation.
The batch tests channel-budget rejection, termination rejection while handles
are active, external decode, the two-argument external delete's original VA
output, stale handles, internal delete, and final termination. Unused context
capacity beyond the size supplied to ExternalCreate must remain A5. All API caller
data/output objects and elementary-stream/PCM buffers carry canaries.

The owned raw39-byte AT9 frame is zero except byte indices4..10:
`07 42 10 84 21 08 42`; configuration bytes are `fe 70 04 c0`. Two consecutive
frames plus8zero lookahead bytes fit the input array. The configuration selects
mono48000Hz,256samples/frame,one frame/superframe. The source-guided frame uses
minimum3bands/10quantization units, zero gradients, CLC5-bit scale factors8,
and24raw9-bit zero spectral coefficients (306bits plus6alignment bits).
This original fixture was separately checked through the real LibAtrac9 decoder;
one frame produces512zero PCM bytes, SHA256
`076a27c79e5ace2a3d47f9dd2e83e4ff6ea8872b3c2218f66c92b89b55f36560`.
It establishes a positive silent decode, not general audio fidelity.

Version2 retains all original checks and adds13tags. An original nonzero39-byte
frame has bytes0..11 `00 00 04 20 07 c6 31 8c 63 18 c6 10`, remaining bytes zero.
Its constant gradient16 and scale factors24 retain precision8; the first raw9-bit
spectral coefficient is+128, remaining23zero. The batch requires nonzero PCM,
39/512consumed/produced counts, guards and untouched output tail. A second
streaming decode must remain nonzero and differ from the first frame, proving
history is retained. ClearContext must reproduce the first frame byte-for-byte.
No platform-specific PCM hash is asserted.

Two separate invalid band-extension inputs are39bytes of zero except first
byte02 or23. Each must fail with zero consumed/produced counts, untouched PCM
and intact guards; ClearContext followed by the valid nonzero frame must recover
the exact initial PCM. These checks require the runtime BEX group-index guard.
They are PC-built probes until separately reviewed and executed on hardware.

Build from WSL with the existing SDK:

```sh
python3 tests/hardware/audiodec_probe/build.py \
  --sdk /work/vitasdk-hardware/vitasdk \
  --output /mnt/c/Users/Tanner/Documents/Codex/work/audiodec-runtime-verification/probe-build-v2
```

The builder uses `-Wall -Wextra -Werror -nostdlib`, validates the Vita ELF, and
audits every import against runtime library/NID exports. It requires exactly the
eleven listed Audiodec imports, writes a hashed manifest, and never installs or
executes the module. Build success alone does not establish hardware behavior.
