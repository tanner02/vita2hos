import re
from pathlib import Path
repo = Path(r"C:/Users/Tanner/Documents/ZCode/work/vita2hos")
db = repo/"external/vita-headers/db/360"
# library name -> nid
libs = {}
for f in db.glob("*.yml"):
    cur = None
    for line in f.read_text(errors="replace").splitlines():
        m = re.match(r"^      ([A-Za-z0-9_]+):\s*$", line)
        if m: cur = m.group(1); continue
        m = re.match(r"^        nid: (0x[0-9A-Fa-f]{8})\s*$", line)
        if m and cur: libs.setdefault(cur, m.group(1).lower())
bad = []
total = 0
for p in (repo/"source/modules").glob("*.c"):
    for m in re.finditer(r'DECLARE_LIBRARY\((\w+),\s*(0x[0-9A-Fa-f]{8})\)', p.read_text(errors="replace")):
        name, nid = m.group(1), m.group(2)
        total += 1
        expect = libs.get(name)
        if expect is None:
            bad.append((p.name, name, nid, "LIBRARY NOT IN DB"))
        elif expect != nid.lower():
            bad.append((p.name, name, nid, f"DB={expect}"))
print("checked", total, "library declarations")
for b in bad: print("MISMATCH:", b)
if not bad: print("all library NIDs consistent")
