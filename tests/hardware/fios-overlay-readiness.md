# FIOS overlay registration evidence

Source baseline: b07996e. The runtime currently implements empty-table
GetList and identity ResolveSync/ResolveWithRangeSync only. No overlay can yet
be installed. Registration must not be added without updating resolution and
GetList at the same time: successful registration followed by unchanged paths
would silently bypass required game file mappings.

The pinned `psp2common/fios2.h` defines `SceFiosOverlay` as 0x258 bytes:

| Offset | Field |
| --- | --- |
| 0 | type: uint8 |
| 1 | order: uint8 |
| 2, 4, 6 | dst_len, src_len, unk2: uint16 |
| 8 | pid: SceUID |
| 12 | id: int32 |
| 16 | dst: 292 bytes |
| 308 | src: 292 bytes |

The public user header leaves `SceFiosKernelOverlay` opaque. A private static
scan of relocated user-owned FIOS code found direct Thumb wrappers for Add,
AddForProcess, Modify and GetInfo that forward arguments to their imports
without constructing a packet. This establishes forwarding at those wrappers;
it does not independently prove the pointed-to user structure equals the
common kernel structure. No ARM code was executed, and no proprietary bytes
or disassembly are stored here.

The pinned common header documents four resolution policies: opaque replaces
the prefix for reads and writes; translucent reads prefer an existing source
and writes use destination; newer reads compare modification times with
specified missing/tie handling; writable reads prefer an existing source and
writes use source. Current SDK documentation also describes ascending order,
application/privileged order ranges and copied per-process records:
https://docs.vitasdk.org/group__SceFios2KernelKernel.html

Before connecting registration to HLE exports, establish the user record ABI
and normalization behavior, retain copied records with process/UID ownership,
keep enumeration and path resolution consistent, and test all four policies
against the existing native file resolver. Include boundary-safe prefix
matching, truncation/error preservation, order filtering, aliases, concurrent
mutation and teardown. Thread-local disable semantics and directory-handle
operations remain separate required compatibility work.
