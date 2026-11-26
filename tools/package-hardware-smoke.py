"""Package a CI Debug artifact and pinned upstream cube samples; execute nothing."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile


SAMPLES = {
    "gxm_cube_sample": "619f2c1bbc1cdf2652025545041ac628675a18628d4fe499074cc23f71da045f",
    "gxm_textured_cube_sample": "04edb34237eec0fd030ceb897d2f810a35f071002da15ae241921296a499355b",
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=Path, help="Downloaded CI Debug artifact ZIP")
    parser.add_argument("--commit", required=True, help="Full runtime commit SHA")
    parser.add_argument("--build-run", required=True, help="GitHub Actions build run URL")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--samples-dir", type=Path, help="Optional local pinned sample ZIPs")
    args = parser.parse_args()
    if len(args.commit) != 40 or any(c not in "0123456789abcdef" for c in args.commit):
        parser.error("--commit must be a full lowercase Git commit SHA")

    repository = Path(__file__).resolve().parent.parent
    artifact_data = args.artifact.read_bytes()
    with zipfile.ZipFile(io.BytesIO(artifact_data)) as archive:
        names = [n for n in archive.namelist() if n.endswith(".nsp")]
        if len(names) != 1:
            raise ValueError("Expected exactly one NSP in the CI artifact")
        nsp = archive.read(names[0])
    if nsp[:4] != b"PFS0":
        raise ValueError("NSP does not have the expected PFS0 header")

    files = {"sd/vita2hos/vita2hos.nsp": nsp}
    samples = []
    for name, expected in SAMPLES.items():
        url = f"https://github.com/xerpi/vita2hos/releases/download/alpha-0.4/{name}.zip"
        if args.samples_dir:
            data = (args.samples_dir / f"{name}.zip").read_bytes()
        else:
            with urllib.request.urlopen(url, timeout=60) as response:
                data = response.read()
        if sha256(data) != expected:
            raise ValueError(f"Pinned sample checksum mismatch: {name}")
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            executable = archive.read(f"{name}.velf")
        if executable[:4] != b"\x7fELF":
            raise ValueError(f"Expected a Vita ELF sample: {name}")
        files[f"samples/{name}/executable"] = executable
        if name == "gxm_cube_sample":
            files["sd/vita2hos/executable"] = executable
        samples.append({"name": name, "url": url, "archive_sha256": expected,
                        "executable_sha256": sha256(executable)})

    files["tools/capture-netlog.ps1"] = (repository / "tools/capture-netlog.ps1").read_bytes()
    instructions = (repository / "tests/hardware/README.md").read_text(encoding="utf-8")
    files["HARDWARE-TEST.md"] = (
        f"Runtime commit: `{args.commit}`\n\nBuild: {args.build_run}\n\n" + instructions
    ).encode("utf-8")
    manifest = {
        "runtime_commit": args.commit,
        "build_run": args.build_run,
        "artifact_sha256": sha256(artifact_data),
        "samples": samples,
        "hardware_execution": "not performed",
        "files": {name: sha256(data) for name, data in files.items()},
    }
    files["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w") as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data, compresslevel=9)
    print(f"Created {args.output} ({args.output.stat().st_size} bytes)")
    print(f"SHA-256: {sha256(args.output.read_bytes())}")


if __name__ == "__main__":
    main()
