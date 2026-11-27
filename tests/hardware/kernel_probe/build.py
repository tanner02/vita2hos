"""Build and audit the owned kernel probe; no deployment or execution."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

def build(sdk, output):
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    output.mkdir(parents=True, exist_ok=True)
    elf = output / "kernel_probe.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-gcc"), "-Os", "-Wall", "-Wextra", "-Werror",
        "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-tree-loop-distribute-patterns",
        "-march=armv7-a", "-marm", "-mfloat-abi=softfp", "-mfpu=neon",
        str(here / "main.c"), "-nostdlib", "-Wl,-q,-e,module_start,-z,noexecstack",
        "-lSceLibKernel_stub", "-lSceKernelThreadMgr_stub", "-lSceIofilemgr_stub", "-lSceSysmem_stub", "-lgcc", "-o", str(elf)], check=True)
    executable = output / "executable"
    creation = subprocess.run([str(sdk / "bin/vita-elf-create"), "-vv", "-e", str(here / "exports.yml"),
        str(elf), str(executable)], check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    imports = re.findall(r"library_nid=(0x[0-9A-Fa-f]+).*?target_nid=(0x[0-9A-Fa-f]+) \((\w+)\)", creation.stdout)
    sources = "\n".join(path.read_text(encoding="utf-8") for path in (repo / "source").rglob("*.c"))
    libraries = {name: int(nid, 16) for name, nid in re.findall(r"DECLARE_LIBRARY\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", sources)}
    exports = {(libraries[name], int(nid, 16)) for name, nid in re.findall(
        r"EXPORT(?:_FUNC)?\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", sources) if name in libraries}
    missing = [name for library, nid, name in imports if (int(library, 16), int(nid, 16)) not in exports]
    if missing:
        raise ValueError("Missing runtime source exports: " + ", ".join(missing))
    spec=importlib.util.spec_from_file_location("guest",repo/"tools/guest_elf.py")
    guest=importlib.util.module_from_spec(spec);spec.loader.exec_module(guest)
    data=executable.read_bytes();summary=guest.validate_guest(data)
    assert summary["function_imports"]==len(imports) and len(imports)>20
    manifest={"hardware_execution":"not performed","function_imports":len(imports),
              "sha256":hashlib.sha256(data).hexdigest(),"bytes":len(data)}
    (output/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(manifest,indent=2))
if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();build(args.sdk,args.output)
