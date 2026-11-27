# Startup callback observer

An optional, owned diagnostic module for a **reviewed table of no-argument,
uint32-returning Thumb startup callbacks**. This is not a general function hook.
It records BEGIN/END and the unchanged raw return value, leaving finalizers and
guest instructions untouched. It does not turn failed initialization into success.

Build with `python3 build.py --sdk /path/to/vitasdk --output /path/to/output`.
The builder checks guest structure and native imports, and never deploys files.
Load the resulting module through an explicit `modules.txt` entry after any
required guest libraries. Restore the original module list after diagnosis.

The operator supplies a private, reviewed `app0:/startup-init.plan`. No game
addresses, plans, binaries, or copyrighted data are included here. The module
checks the exact source executable's FNV-1a-64 fingerprint, then validates every
record and original word before changing any callback pointer. FNV is a mistake
guard, not authentication: the plan and its declared memory ranges are trusted
diagnostic input. They must correspond to the loaded executable's layout.
Module startup must complete before any thread can call the observed table.

Plan format (little endian): a 32-byte header with magic `STI1`, source FNV64 at
offset 4, record count at 12, then code base/size and data base/size at 16/20/24/28.
Each of at most 64 records occupies 56 bytes: pointer-slot address, original Thumb
callback address, then a 48-byte printable ASCII name with a NUL terminator.
Slots must be distinct aligned data words; originals must lie in the declared
code region. There must be no trailing bytes. Source path is `app0:/executable`.

The fresh report is `app0:/startup-init-v1.txt`. GUARD must be zero. A BEGIN with
no END means the callback has not returned; a zero END is only failure if the
reviewed dispatcher interprets zero that way. After installation, report I/O is
best effort so a logging failure cannot change the original callback result.
Each line is opened, appended, and closed separately to preserve prior results
if the guest subsequently crashes. This adds startup overhead and is unsuitable
for performance measurement. It is resident until process exit and must not be
unloaded while the table contains its wrappers.
