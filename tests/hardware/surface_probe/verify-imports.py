"""Audit the built guest's import stubs against the current runtime source exports."""
import argparse
from pathlib import Path
import re
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sdk", type=Path, required=True)
parser.add_argument("--build-dir", type=Path, required=True)
args = parser.parse_args()
here = Path(__file__).resolve().parent
repo = here.parents[2]
sources = "\n".join(path.read_text(encoding="utf-8") for path in (repo / "source").rglob("*.c"))
libraries = {name: int(nid, 16) for name, nid in
             re.findall(r"DECLARE_LIBRARY\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", sources)}
exports = {(libraries[name], int(nid, 16)) for name, nid in
           re.findall(r"EXPORT(?:_FUNC)?\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", sources)
           if name in libraries}
result = subprocess.run([str(args.sdk / "bin/vita-elf-create"), "-vv", "-e", str(here / "exports.yml"),
                         str(args.build_dir / "surface_probe.elf"),
                         str(args.build_dir / "validation.velf")],
                        check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
imports = re.findall(r"library_nid=(0x[0-9A-Fa-f]+).*?target_nid=(0x[0-9A-Fa-f]+) \((\w+)\)",
                     result.stdout)
if not imports:
    raise SystemExit("No guest import records found")
missing = [name for library, nid, name in imports if (int(library, 16), int(nid, 16)) not in exports]
if missing:
    raise SystemExit("Missing runtime source exports: " + ", ".join(missing))
data = (args.build_dir / "executable").read_bytes()
if len(data) < 52 or data[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<HH", data, 16) != (0xfe04, 40):
    raise SystemExit("Expected an ELF32 ARM Vita executable")
entry, phoff = struct.unpack_from("<II", data, 24)
phsize, phcount = struct.unpack_from("<HH", data, 42)
index, relative = entry >> 30, entry & 0x3fffffff
if phsize != 32 or index >= phcount or phoff + phsize * phcount > len(data):
    raise SystemExit("Invalid module-info program header")
kind, offset, _, _, size, memory, flags, _ = struct.unpack_from("<8I", data, phoff + index * phsize)
if kind != 1 or not flags & 1 or offset + size > len(data) or relative + 0x5c > size:
    raise SystemExit("Module info must reside in a file-backed executable segment")
info = offset + relative
first, last = struct.unpack_from("<II", data, info + 0x2c)
start = struct.unpack_from("<I", data, info + 0x44)[0]
if start == 0xffffffff or start & ~1 >= memory:
    raise SystemExit("Guest has no valid module entry point; check imagemodule configuration")
if first % 4 or first >= last or last > size:
    raise SystemExit("Guest has no valid runtime import range")
count = 0
while first < last:
    record_size, _, _, functions = struct.unpack_from("<4H", data, offset + first)
    if record_size not in (0x24, 0x34) or record_size > last - first:
        raise SystemExit("Invalid generated runtime import record")
    count += functions
    first += record_size
if count != len(imports): raise SystemExit("Generated runtime imports disagree with input stubs")
print(f"Verified guest entry point, import range, and {count} runtime source exports; hardware execution not performed.")
