"""Build the owned real-libc TLS probe. No target execution or deployment."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def build(sdk, output):
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    output.mkdir(parents=True, exist_ok=True)
    elf = output / "libc_tls_probe.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-gcc"), "-Os", "-Wall", "-Wextra", "-Werror",
        "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-tree-loop-distribute-patterns",
        "-march=armv7-a", "-marm", "-mfloat-abi=softfp", "-mfpu=neon",
        str(here / "main.c"), "-nostdlib", "-Wl,-q,-e,module_start,-z,noexecstack",
        "-lSceLibKernel_stub", "-lSceKernelThreadMgr_stub", "-lSceIofilemgr_stub",
        "-lSceLibc_stub", "-lgcc", "-o", str(elf)], check=True)
    executable = output / "executable"
    creation = subprocess.run([str(sdk / "bin/vita-elf-create"), "-vv", "-e", str(here / "exports.yml"),
        str(elf), str(executable)], check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    imports = re.findall(r"library_nid=(0x[0-9A-Fa-f]+).*?target_nid=(0x[0-9A-Fa-f]+) \((\w+)\)", creation.stdout)
    external = (0xbe43bb07, 0xbc529b7b)
    assert [(int(lib, 16), int(nid, 16)) for lib, nid, name in imports if name == "__tls_get_addr"] == [external]
    sources = "\n".join(p.read_text(encoding="utf-8") for p in (repo / "source").rglob("*.c"))
    libraries = {name: int(nid, 16) for name, nid in re.findall(r"DECLARE_LIBRARY\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", sources)}
    native = {(libraries[name], int(nid, 16)) for name, nid in re.findall(
        r"EXPORT(?:_FUNC)?\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)", sources) if name in libraries}
    missing = [name for lib, nid, name in imports if (int(lib, 16), int(nid, 16)) not in native | {external}]
    if missing:
        raise ValueError("Missing runtime imports: " + ", ".join(missing))
    data = bytearray(executable.read_bytes())
    guest = load_module(repo / "tools/guest_elf.py", "libc_probe_guest")
    summary = guest.validate_guest(data)
    assert summary["function_imports"] == len(imports) == 10
    tls_builder = load_module(here.parent / "tls_probe/build.py", "tls_probe_builder")
    plans = load_module(repo / "tools/make-tls-plan.py", "libc_probe_plan_reader")
    segments = plans.segments(data)
    symbols = tls_builder.symbols(data) or tls_builder.symbols(elf.read_bytes())
    entry = struct.unpack_from("<I", data, 24)[0]
    index, relative = entry >> 30, entry & 0x3fffffff
    base, _, content = segments[index]
    assert struct.unpack_from("<3I", content, relative + 0x38) == (0, 0, 0)
    address, size = symbols["probe_tls_seed"]
    start = address - base
    assert size == 4 and 0 <= start <= len(content) - 4
    assert content[start:start+4] == bytes.fromhex("df9b5713")
    phoff = struct.unpack_from("<I", data, 28)[0]
    segment_offset = struct.unpack_from("<I", data, phoff + index * 32 + 4)[0]
    struct.pack_into("<3I", data, segment_offset + relative + 0x38, start, 4, 16)
    assert guest.validate_guest(data)["function_imports"] == 10
    executable.write_bytes(data)
    # Explicit list disables automatic discovery of unrelated guest modules.
    (output / "modules.txt").write_text("sce_module/libc.suprx\n", encoding="utf-8")
    manifest = {"probe": "libc_tls_probe", "version": 1, "hardware_execution": "not performed",
        "bytes": len(data), "executable_sha256": hashlib.sha256(data).hexdigest(),
        "tls_initialized_bytes": 4, "tls_total_bytes": 16,
        "imports": [{"library": lib, "nid": nid, "name": name} for lib, nid, name in imports],
        "external_provider": "User-supplied matching libc.suprx and its source-bound TLS plan; not bundled",
        "expected_report": "/vita2hos/libc-tls-probe.txt", "expected_guest_exit": 0,
        "main_tls_plan": "Absent: owned main has no thread-pointer instructions to patch"}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.sdk, args.output)
