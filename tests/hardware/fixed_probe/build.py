"""Build an owned fixed-address ARM guest; never execute the output."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess


def build(sdk, output):
    here = Path(__file__).resolve().parent
    output.mkdir(parents=True, exist_ok=True)
    obj, elf = output / "fixed_probe.o", output / "fixed_probe.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-gcc"), "-c", "-march=armv7-a", "-marm",
                    str(here / "main.S"), "-o", str(obj)], check=True)
    subprocess.run([str(sdk / "bin/arm-vita-eabi-ld"), "-z", "max-page-size=4096",
                    "-T", str(here / "fixed.ld"), str(obj), "-o", str(elf)], check=True)
    data = bytearray(elf.read_bytes())
    phoff = struct.unpack_from("<I", data, 28)[0]
    phsize, count = struct.unpack_from("<HH", data, 42)
    assert phsize == 32 and count == 2
    headers = [struct.unpack_from("<8I", data, phoff + i * phsize) for i in range(count)]
    assert [p[2] for p in headers] == [0x81000000, 0x81879000]
    assert [p[6] for p in headers] == [5, 6]
    assert headers[1][4] == 4 and headers[1][5] == 8
    # The linker resolves all absolute references at the declared VAs. Vita's
    # e_entry encodes the module-info segment index and offset, not a function VA.
    entry = struct.unpack_from("<I", data, 24)[0]
    assert headers[0][2] <= entry < headers[0][2] + headers[0][4]
    struct.pack_into("<H", data, 16, 0xfe00)
    struct.pack_into("<I", data, 24, entry - headers[0][2])
    repo = here.parents[2]
    spec = importlib.util.spec_from_file_location("guest", repo / "tools/guest_elf.py")
    guest = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guest)
    info = guest.validate_guest(data, allow_fixed=True)
    assert info["function_imports"] == 1 and info["entry_offset"] == 0
    target = output / "executable"
    target.write_bytes(data)
    manifest = {"type": "ET_SCE_EXEC", "code_address": "0x81000000",
                "data_address": "0x81879000", "sha256": hashlib.sha256(data).hexdigest(),
                "expected_guest_exit": 0, "hardware_execution": "not performed",
                "failure_status": {"1": "initialized data mismatch", "2": "BSS not zero", "3": "data write/read mismatch", "4": "code address mismatch"}}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Built fixed-address probe (ELF32 ARM, code/data references resolved at link time).")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.sdk, args.output)
