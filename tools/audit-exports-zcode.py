import re, struct
from pathlib import Path

repo = Path(r"C:/Users/Tanner/Documents/ZCode/work/vita2hos")
db = repo/"external/vita-headers/db/360"

# nid -> (name, library_name) from db (functions only)
fn = {}
for f in db.glob("*.yml"):
    lib = None
    for line in f.read_text(errors="replace").splitlines():
        m = re.match(r"^      ([A-Za-z0-9_]+):\s*$", line)
        if m: lib = m.group(1); continue
        m = re.match(r"^          ([A-Za-z0-9_]+): (0x[0-9A-Fa-f]{8})\s*$", line)
        if m: fn.setdefault(m.group(2).lower(), (m.group(1), lib))

# parse all EXPORT(...) in my files
files = ["SceAppUtil.c","SceNpManager.c","SceNpCommon.c","SceNpSignaling.c","SceNpActivity.c",
         "SceSysmodule.c","SceProcessmgr.c","SceLibKernel.c","SceNet.c","SceLibHttp.c","SceLibSsl.c",
         "SceCommonDialog.c","SceNgs.c","SceAvcodec.c","SceNpUtility.c","SceNpBasic.c","SceNpMessage.c",
         "SceCamera.c","SceIme.c","SceAudioInOut.c","SceFiber.c","SceNearUtil.c","SceSystemMisc.c",
         "SceFios2Kernel.c","SceRtc.c","SceTouch.c"]
bad = []
total = 0
for fname in files:
    p = repo/"source/modules"/fname
    if not p.exists(): continue
    for m in re.finditer(r'EXPORT\((\w+),(0x[0-9A-Fa-f]{8}),[^,]+,(\w+)', p.read_text(errors="replace")):
        lib, nid, name = m.group(1), m.group(2), m.group(3)
        total += 1
        entry = fn.get(nid.lower())
        if entry is None:
            bad.append((fname, lib, nid, name, "NID NOT IN DB"))
        elif entry[0] != name:
            bad.append((fname, lib, nid, name, f"DB NAME={entry[0]}"))
        elif entry[1] != lib:
            bad.append((fname, lib, nid, name, f"DB LIBRARY={entry[1]}"))
print("checked", total, "exports")
if bad:
    for b in bad: print("MISMATCH:", b)
else:
    print("all names and NIDs consistent with vitasdk db/360")
