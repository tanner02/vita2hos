"""Build and audit the owned TLS probe; never execute it or deploy files."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def symbols(data):
    first = struct.unpack_from("<I", data, 32)[0]
    stride, count = struct.unpack_from("<HH", data, 46)
    if not first or stride != 40:
        return {}
    sections = [struct.unpack_from("<10I", data, first + i * stride) for i in range(count)]
    result = {}
    for section in sections:
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for offset in range(section[4], section[4] + section[5], section[9]):
            name, address, size, info, _, _ = struct.unpack_from("<IIIBBH", data, offset)
            if info & 15 in (1, 2):
                result[names[name:].split(b"\0", 1)[0].decode()] = address, size
    return result


def build(sdk, output):
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    output.mkdir(parents=True, exist_ok=True)
    elf = output / "tls_probe.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-gcc"), "-Os", "-Wall", "-Wextra", "-Werror",
        "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-tree-loop-distribute-patterns",
        "-march=armv7-a", "-marm", "-mfloat-abi=softfp", "-mfpu=neon",
        str(here / "main.c"), str(here / "thread_pointer.S"), "-nostdlib", "-Wl,-q,-e,module_start,-z,noexecstack",
        "-lSceLibKernel_stub", "-lSceKernelThreadMgr_stub", "-lSceIofilemgr_stub", "-lgcc", "-o", str(elf)], check=True)
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
    data = bytearray(executable.read_bytes())
    guest = module(repo / "tools/guest_elf.py", "guest")
    assert guest.validate_guest(data)["function_imports"] == len(imports) == 10
    plan_tool = module(repo / "tools/make-tls-plan.py", "plan_tool")
    loaded = plan_tool.segments(data)
    function_symbols = symbols(data) or symbols(elf.read_bytes())
    sites = []
    for name, mode, word, size in [("probe_read_tp_arm", "arm", 0xee1d0f70, 8),
                                   ("probe_read_tp_thumb", "thumb", 0x0f70ee1d, 6)]:
        address, actual_size = function_symbols[name]
        assert actual_size == size
        address &= ~1
        matches = [(index, address - base, mode) for index, (base, flags, content) in loaded.items()
                   if flags & 1 and base <= address and address - base + 4 <= len(content)]
        assert len(matches) == 1
        index, offset, _ = matches[0]
        assert struct.unpack_from("<I", loaded[index][2], offset)[0] == word
        sites.append(matches[0])
    entry = struct.unpack_from("<I", data, 24)[0]
    info = loaded[entry >> 30][2][entry & 0x3fffffff:]
    start, filesz, memsz = struct.unpack_from("<3I", info, 0x38)
    assert (start, filesz, memsz) == (0, 0, 0)
    # The pinned converter does not populate Sony TLS fields from compiler TLS
    # sections. Encode the owned template explicitly, as in the fixed probe's
    # explicit module metadata; never patch any supplied target binary.
    seed_address, seed_size = function_symbols["probe_tls_seed"]
    assert seed_size == 4
    base = loaded[entry >> 30][0]
    start = seed_address - base
    assert 0 <= start and start + 4 <= len(loaded[entry >> 30][2])
    filesz, memsz = 4, 16
    phoff = struct.unpack_from("<I", data, 28)[0]
    segment_offset = struct.unpack_from("<I", data, phoff + (entry >> 30) * 32 + 4)[0]
    info_offset = segment_offset + (entry & 0x3fffffff)
    struct.pack_into("<3I", data, info_offset + 0x38, start, filesz, memsz)
    executable.write_bytes(data)
    assert guest.validate_guest(data)["function_imports"] == 10
    seed = loaded[entry >> 30][2][start:start + filesz]
    print(f"TLS template: start=0x{start:x}, initialized={filesz}, total={memsz}, seed={seed.hex()}")
    assert filesz == 4 and memsz == 16 and seed == bytes.fromhex("df9b5713")
    plan = plan_tool.make_plan(data, sites)
    (output / "executable.tlsplan").write_bytes(plan)
    manifest = dict(probe="tls_probe", version=1, executable_sha256=hashlib.sha256(data).hexdigest(),
        plan_sha256=hashlib.sha256(plan).hexdigest(), tls_initialized_bytes=filesz, tls_total_bytes=memsz,
        patch_sites=[dict(segment=i, offset=hex(o), mode=m) for i, o, m in sites],
        imports=[name for _, _, name in imports], expected_guest_exit=0,
        expected_report="/vita2hos/tls-probe.txt", hardware_execution="not performed",
        checks=["ARM and Thumb reads", "template/BSS", "native calls and 32 yields per thread",
                "two-thread isolation", "normal worker return", "main pointer after worker join"],
        wait_failure="Records failure and parks; close the title if worker join cannot be verified.")
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Built owned TLS probe, verified 10 imports, template layout, and two symbol-derived patch sites.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.sdk, args.output)
