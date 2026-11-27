"""Audit the built guest's import stubs against the current runtime source exports."""
import argparse
from pathlib import Path
import re
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
print(f"Verified {len(imports)} guest imports against runtime source exports; hardware execution not performed.")
