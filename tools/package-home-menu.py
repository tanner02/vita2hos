"""Build a standalone installable homebrew title from a verified runtime bundle.

Packaging keys are used only locally and are never copied into outputs.
This tool builds files; it does not install or launch software on a console.
"""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile

spec = importlib.util.spec_from_file_location("title_metadata", Path(__file__).with_name("package-title-profile.py"))
metadata = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metadata)
DEFAULT_PROGRAM_ID = "0100f27432680000"


def title_id(value):
    result = metadata.program_id(value)
    if result & 0xfff:
        raise ValueError("Standalone base application ID must end in 000")
    return result


def validate_icon(data):
    if data[:2] != b"\xff\xd8" or data[-2:] != b"\xff\xd9":
        raise ValueError("Icon must be a JPEG")
    position = 2
    while position + 4 <= len(data):
        if data[position] != 0xff: break
        marker = data[position + 1]
        size = int.from_bytes(data[position + 2:position + 4], "big")
        if size < 2 or position + 2 + size > len(data): break
        if marker in (0xc0, 0xc1, 0xc2):
            if size < 8 or struct.unpack_from(">HH", data, position + 5) != (256, 256):
                raise ValueError("Icon must be 256x256")
            return
        position += 2 + size
    raise ValueError("JPEG icon has no supported frame header")


def runtime_files(bundle, program):
    with zipfile.ZipFile(bundle) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        runtime = archive.read("sd/vita2hos/vita2hos.nsp")
    if hashlib.sha256(runtime).hexdigest() != manifest["files"].get("sd/vita2hos/vita2hos.nsp"):
        raise ValueError("Runtime bundle checksum mismatch")
    patched = metadata.retarget(runtime, program)
    files = {name: patched[start:end] for name, (start, end) in metadata.pfs_files(patched).items()}
    return manifest, files


def installed_contents(data):
    if len(data) < 16 or data[:4] != b"PFS0": raise ValueError("Missing installable NSP container")
    count, strings = struct.unpack_from("<II", data, 4)
    end = 16 + count * 24
    payload = end + strings
    if count != 3 or payload > len(data): raise ValueError("Expected program, control and meta NCAs")
    names = data[end:payload]
    result, spans = {}, []
    for i in range(count):
        offset, size, name_offset = struct.unpack_from("<QQI", data, 16 + i * 24)
        if name_offset >= len(names) or b"\0" not in names[name_offset:]:
            raise ValueError("Invalid NSP filename")
        name = names[name_offset:].split(b"\0", 1)[0].decode("ascii")
        stem = name.removesuffix(".cnmt.nca") if name.endswith(".cnmt.nca") else name.removesuffix(".nca")
        if (len(stem) != 32 or any(c not in "0123456789abcdef" for c in stem) or
                not name.endswith(".nca") or name in result or size < 0xc00):
            raise ValueError("Unexpected installable NSP content")
        start, stop = payload + offset, payload + offset + size
        if stop > len(data) or any(start < b and a < stop for a, b in spans):
            raise ValueError("Out-of-bounds or overlapping NCA")
        digest = hashlib.sha256(data[start:stop]).hexdigest()
        if stem != digest[:32]: raise ValueError("NCA filename hash mismatch")
        result[name] = digest
        spans.append((start, stop))
    if sum(name.endswith(".cnmt.nca") for name in result) != 1:
        raise ValueError("Expected exactly one content meta NCA")
    return result


def run_tool(command, directory):
    result = subprocess.run([str(arg) for arg in command], cwd=directory,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        # Packer diagnostics can contain key material; never print raw output.
        errors = []
        for line in result.stdout.decode("utf-8", errors="replace").splitlines():
            if line.startswith(("Error:", "Failed to", "Invalid ")):
                errors.append("Required packaging key unavailable" if "key" in line.lower() else
                              re.sub(r"[0-9a-fA-F]{16,}", "<redacted>", line))
        raise RuntimeError(f"{Path(command[0]).name} failed with status {result.returncode}: " +
                           "; ".join(errors))


def packer_keyset(source, destination):
    # Old packers reject newer, unrelated key definitions. Supply only the two
    # validated fields needed for a key-generation-1 homebrew title.
    keys = {}
    required = {"header_key": 64, "key_area_key_application_00": 32}
    for line in source.read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.partition("=")
        name, value = name.strip(), value.split("#", 1)[0].strip()
        if separator and name in required:
            if name in keys or len(value) != required[name] or not re.fullmatch("[0-9a-fA-F]+", value):
                raise ValueError("Invalid or duplicate required packaging key")
            keys[name] = value
    if keys.keys() != required.keys(): raise ValueError("Required packaging keys unavailable")
    destination.touch(mode=0o600)
    destination.write_text("\n".join(name + " = " + value for name, value in keys.items()) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--program-id", default=DEFAULT_PROGRAM_ID)
    parser.add_argument("--packer", type=Path, required=True, help="hacBrewPack executable")
    parser.add_argument("--nacptool", type=Path, required=True)
    parser.add_argument("--keyset", type=Path, required=True)
    parser.add_argument("--icon", type=Path, default=Path(__file__).resolve().parents[1] / "assets/home-menu-icon.jpg",
                        help="256x256 JPEG; defaults to the project icon")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    program = title_id(args.program_id)
    for path in (args.packer, args.nacptool, args.keyset):
        if not path.is_file(): raise ValueError("Required packaging input missing: " + str(path))
    manifest, files = runtime_files(args.bundle, program)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() or args.output.with_suffix(".json").exists():
        raise ValueError("Output already exists; use a new path")
    with tempfile.TemporaryDirectory(prefix="vita2hos-home-title-") as temp:
        root = Path(temp)
        exefs, control, output = root / "exefs", root / "control", root / "output"
        for folder in (exefs, control, output): folder.mkdir()
        local_keys = root / "packaging-keys.dat"
        packer_keyset(args.keyset, local_keys)
        for name, data in files.items(): (exefs / name).write_bytes(data)
        run_tool([args.nacptool.resolve(), "--create", "vita2hos", "vita2hos contributors", "0.0.3",
                  control / "control.nacp", f"--titleid={program:016x}"], root)
        if args.icon:
            icon = args.icon.read_bytes()
            validate_icon(icon)
            (control / "icon_AmericanEnglish.dat").write_bytes(icon)
        run_tool([args.packer.resolve(), "--keyset", local_keys,
                  "--exefsdir", exefs, "--controldir", control, "--nspdir", output,
                  "--ncadir", root / "nca", "--tempdir", root / "temp",
                  "--backupdir", root / "backup", "--noromfs", "--nologo", "--keygeneration", "1"], root)
        nsp = output / f"{program:016x}.nsp"
        data = nsp.read_bytes()
        contents = installed_contents(data)
        # Confirm packaging did not change the executable's architecture/identity.
        post = (exefs / "main.npdm").read_bytes()
        if post[:4] != b"META" or post[12] & 15: raise ValueError("Packaged process is not ARM32")
        aci = struct.unpack_from("<I", post, 0x70)[0]
        if struct.unpack_from("<Q", post, aci + 0x10)[0] != program:
            raise ValueError("Packaged program ID changed")
        if (exefs / "main").read_bytes() != files["main"]:
            raise ValueError("Runtime NSO changed during packaging")
        shutil.copyfile(nsp, args.output)
    evidence = {"runtime_commit": manifest["runtime_commit"], "program_id": f"{program:016x}",
                "display_name": "vita2hos", "process_architecture": "ARM32 / 32-bit address space",
                "package_type": "installable homebrew NSP", "nsp_sha256": hashlib.sha256(data).hexdigest(),
                "content_sha256": contents, "hardware_execution": "not performed",
                "packer_sha256": hashlib.sha256(args.packer.read_bytes()).hexdigest()}
    args.output.with_suffix(".json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(f"Created {args.output} ({len(data)} bytes); title {program:016x}")
    print("Three content archives verified. Installation and hardware launch not performed.")


if __name__ == "__main__": main()
