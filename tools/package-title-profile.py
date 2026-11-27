"""Prepare a title-specific ARM32 ExeFS replacement without changing hbmenu."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile


def program_id(value):
    if len(value) != 16 or any(c not in "0123456789abcdefABCDEF" for c in value):
        raise ValueError("Program ID must contain exactly 16 hexadecimal digits")
    result = int(value, 16)
    if not 0x0100000000010000 <= result <= 0x01FFFFFFFFFFFFFF:
        raise ValueError("Use an installed application program ID, not a system program")
    return result


def pfs_files(data):
    if len(data) < 16 or data[:4] != b"PFS0":
        raise ValueError("Expected an ExeFS PFS0 container")
    count, strings = struct.unpack_from("<II", data, 4)
    table_end = 16 + count * 24
    payload = table_end + strings
    if count != 2 or payload > len(data):
        raise ValueError("Expected exactly main and main.npdm")
    names = data[table_end:payload]
    files, ranges = {}, []
    for i in range(count):
        offset, size, name_offset = struct.unpack_from("<QQI", data, 16 + i * 24)
        if name_offset >= len(names) or b"\0" not in names[name_offset:]:
            raise ValueError("Invalid PFS0 filename")
        name = names[name_offset:].split(b"\0", 1)[0].decode("ascii")
        start, end = payload + offset, payload + offset + size
        if name in files or end > len(data) or any(start < b and a < end for a, b in ranges):
            raise ValueError("Invalid or overlapping PFS0 entries")
        files[name] = (start, end)
        ranges.append((start, end))
    if set(files) != {"main", "main.npdm"}:
        raise ValueError("Unexpected ExeFS modules")
    start, end = files["main"]
    if end - start < 0x100 or data[start:start + 4] != b"NSO0":
        raise ValueError("Missing runtime NSO")
    return files


def retarget(data, title_id):
    files = pfs_files(data)
    start, end = files["main.npdm"]
    meta = data[start:end]
    if len(meta) < 0x80 or meta[:4] != b"META" or meta[12] & 15:
        raise ValueError("Expected ARM32 metadata with 32-bit address space")
    aci, aci_size, acid, acid_size = struct.unpack_from("<IIII", meta, 0x70)
    if (aci < 0x80 or acid < 0x80 or aci_size < 0x38 or acid_size < 0x240 or
            aci + aci_size > len(meta) or acid + acid_size > len(meta) or
            (aci < acid + acid_size and acid < aci + aci_size) or
            meta[aci:aci + 4] != b"ACI0" or meta[acid + 0x200:acid + 0x204] != b"ACID"):
        raise ValueError("Invalid NPDM access-control regions")
    if any(meta[acid:acid + 0x100]):
        raise ValueError("Refusing to modify a signed NPDM")
    result = bytearray(data)
    for offset in (aci + 0x10, acid + 0x210, acid + 0x218):
        struct.pack_into("<Q", result, start + offset, title_id)
    return bytes(result)


def build_profile(bundle, title_id):
    title = program_id(title_id)
    with zipfile.ZipFile(bundle) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        runtime = archive.read("sd/vita2hos/vita2hos.nsp")
    source_hash = hashlib.sha256(runtime).hexdigest()
    if manifest["files"].get("sd/vita2hos/vita2hos.nsp") != source_hash:
        raise ValueError("Runtime checksum mismatch")
    target = f"sd/atmosphere/contents/{title:016x}/exefs.nsp"
    profile = retarget(runtime, title)
    instructions = (
        "Experimental title-specific vita2hos launch profile. This route failed to launch on the tested Switch.\n\n"
        f"Host application program ID: {title:016x}\nRuntime: {manifest['runtime_commit']}\n\n"
        "Preserve normal hbmenu configuration: atmosphere/hbl.nsp, R application override, 39_bit.\n"
        "Install the single file below sd/ only if this title has no existing ExeFS overrides.\n"
        "Intended behavior: normal host launch runs vita2hos in ARM32; R opens normal hbmenu. Launch compatibility is not established.\n"
        "Other applications and Album retain their existing launch configuration.\n"
        "The runtime still reads /vita2hos/executable and /vita2hos/ game assets.\n\n"
        "Rollback: close the application, then rename the installed exefs.nsp to\n"
        "exefs.vita2hos-disabled.nsp (or remove only this new file). The host game then launches normally.\n"
        "Do not replace the global override_config.ini or install this ExeFS NSP as a title.\n\n"
        "Loader semantics: https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/loader.md\n"
    ).encode()
    files = {target: profile, "TITLE-PROFILE.txt": instructions}
    files["manifest.json"] = (json.dumps({
        "runtime_commit": manifest["runtime_commit"], "host_program_id": f"{title:016x}",
        "source_runtime_sha256": source_hash, "hardware_execution": "not performed",
        "files": {name: hashlib.sha256(value).hexdigest() for name, value in files.items()},
    }, indent=2) + "\n").encode()
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--program-id", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    files = build_profile(args.bundle, args.program_id)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, value in files.items(): archive.writestr(name, value)
    print("Created:", args.output)
    print("No SD card or global homebrew configuration was modified.")


if __name__ == "__main__":
    main()
