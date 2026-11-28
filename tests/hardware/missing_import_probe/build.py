"""Build/audit owned missing-import callers; never deploy or execute."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess


def build(sdk, output, media_probe=False):
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, VITASDK=str(sdk))
    env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
    stubs = output / "stubs"
    subprocess.run([str(sdk / "bin/vita-libs-gen"), str(here / "imports.yml"), str(stubs)], check=True, env=env)
    subprocess.run(["make", "-C", str(stubs), "-j4"], check=True, env=env)
    elf = output / "missing_import_probe.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-gcc"), "-Os", "-Wall", "-Wextra", "-Werror",
        "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-march=armv7-a",
        *(["-DMEDIA_PROBE"] if media_probe else []),
        "-marm", "-mfloat-abi=softfp", "-mfpu=neon", "-nostdlib",
        "-Wl,-q,-e,module_start,-z,noexecstack", str(here / "main.c"), str(here / "calls.S"),
        "-L" + str(stubs), "-lowned_missing_stub", "-lSceLibKernel_stub", "-lSceIofilemgr_stub",
        "-lSceSysmodule_stub", "-lgcc", "-o", str(elf)], check=True, env=env)
    guest = output / "executable"
    result = subprocess.run([str(sdk / "bin/vita-elf-create"), "-vv", "-e", str(here / "exports.yml"),
        str(elf), str(guest)], check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
    (output / "creation.txt").write_text(result.stdout, encoding="utf-8")
    imports = re.findall(r"library_nid=(0x[0-9A-Fa-f]+).*?target_nid=(0x[0-9A-Fa-f]+) \((\w+)\)", result.stdout)
    source = "\n".join(p.read_text(encoding="utf-8") for p in (repo / "source").rglob("*.c"))
    libraries = {n: int(v, 16) for n, v in re.findall(r"DECLARE_LIBRARY\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", source)}
    exports = {(libraries[n], int(v, 16)) for n, v in re.findall(r"EXPORT(?:_FUNC)?\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", source) if n in libraries}
    missing = {(int(lib, 16), int(nid, 16)) for lib, nid, _ in imports if (int(lib, 16), int(nid, 16)) not in exports}
    expected = {(0x6f776e74, 0xaabb1011), (0x6f776e74, 0xaabb1022)}
    assert missing == expected, missing
    assert not any(key in exports for key in expected)
    spec = importlib.util.spec_from_file_location("guest", repo / "tools/guest_elf.py")
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    data = guest.read_bytes()
    assert tool.validate_guest(data)["function_imports"] == len(imports)
    disassembly = subprocess.check_output([str(sdk / "bin/arm-vita-eabi-objdump"), "-d", str(elf)], text=True)
    (output / "disassembly.txt").write_text(disassembly, encoding="utf-8")
    for symbol in ("call_missing_arm", "call_missing_thumb"):
        assert "<" + symbol + ">:" in disassembly
    manifest = {"probe": "missing_import_media_probe" if media_probe else "missing_import_probe", "version": 2 if media_probe else 1, "runtime_minimum": "media-probe gate support" if media_probe else "9a7eb87",
        "hardware_execution": "not performed", "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        "function_imports": len(imports), "expected_unresolved_pairs": [list(x) for x in sorted(expected)],
        "expected_first_use_records": 2, "expected_report": "/vita2hos/missing-import-media-probe-v2.txt" if media_probe else "/vita2hos/missing-import-probe.txt"}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--media-probe", action="store_true")
    args = parser.parse_args()
    build(args.sdk, args.output, args.media_probe)
