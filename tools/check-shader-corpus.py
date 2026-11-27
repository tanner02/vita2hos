"""Run bounded, isolated shader translation checks; no deployment or GPU execution.

Keep reports for proprietary corpora outside the repository: translator diagnostics
can contain private program details. A translation pass is not a rendering pass.
"""
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("translator", type=Path)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=32)
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("--uam", type=Path, help="also compile generated GLSL with this UAM executable")
    args = parser.parse_args()
    if not 1 <= args.limit <= 1024 or args.offset < 0 or not 0 < args.timeout <= 120:
        parser.error("require limit 1..1024, nonnegative offset and timeout 0..120 seconds")
    translator = args.translator.resolve(strict=True)
    uam = args.uam.resolve(strict=True) if args.uam else None
    uam_hash = hashlib.sha256(uam.read_bytes()).hexdigest() if uam else None
    paths = sorted(args.corpus.glob("*.gxp"))[args.offset:args.offset + args.limit]
    if not paths:
        parser.error("selected corpus range is empty")
    results = []
    translator_hash = hashlib.sha256(translator.read_bytes()).hexdigest()
    limits = None
    if os.name == "posix":
        import resource
        def limits():
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    for path in paths:
        row = {"file": path.name, "bytes": path.stat().st_size}
        if not 160 <= row["bytes"] <= 64 * 1024 * 1024:
            row["status"] = "input_size_rejected"
        else:
            row["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            try:
                with tempfile.TemporaryDirectory(prefix="vita2hos-shader-") as temporary:
                    glsl = Path(temporary) / "program.glsl"
                    command = [str(translator), "--single", str(path.resolve())]
                    if uam:
                        command.append(str(glsl))
                    run = subprocess.run(command, capture_output=True, timeout=args.timeout,
                                         preexec_fn=limits)
                    row.update(returncode=run.returncode,
                               stdout=run.stdout.decode("utf-8", "replace"),
                               stderr=run.stderr.decode("utf-8", "replace"))
                    marker = re.search(r"^TRANSLATED stage=(fragment|vertex) uniform_bytes=\d+ glsl_bytes=\d+$",
                                       row["stdout"], re.MULTILINE)
                    row["status"] = "translated" if run.returncode == 0 and marker else "failed"
                    if uam and row["status"] == "translated":
                        row["stage"] = marker[1]
                        row["status"] = "compiler_running"
                        dksh = Path(temporary) / "program.dksh"
                        compiled = subprocess.run([str(uam), "-s", "frag" if marker[1] == "fragment" else "vert",
                                                   "-o", str(dksh), str(glsl)], capture_output=True,
                                                  timeout=args.timeout, preexec_fn=limits)
                        row["uam"] = {"returncode": compiled.returncode,
                                      "stdout": compiled.stdout.decode("utf-8", "replace"),
                                      "stderr": compiled.stderr.decode("utf-8", "replace")}
                        row["status"] = "compile_failed"
                        if compiled.returncode == 0:
                            row["status"] = "invalid_dksh"
                            if dksh.is_file() and 24 <= dksh.stat().st_size <= 64 * 1024 * 1024:
                                data = dksh.read_bytes()
                                header = struct.unpack_from("<6I", data)
                                # Match the production wrapper's structural checks.
                                if (data[:4] == b"DKSH" and 24 <= header[1] <= header[2] <= len(data)
                                        and header[3] == len(data) - header[2]):
                                    row["status"] = "compiled"
                                    row["dksh_bytes"] = len(data)
                                    row["dksh_sha256"] = hashlib.sha256(data).hexdigest()
            except subprocess.TimeoutExpired:
                row["status"] = "compile_timeout" if row.get("status") == "compiler_running" else "timeout"
            except OSError as error:
                row.update(status="launch_error", stderr=str(error))
        results.append(row)
        # Checkpoint after every program, including failures and timeouts.
        args.output.parent.mkdir(parents=True, exist_ok=True)
        report = {"translator_sha256": translator_hash,
                  "uam_sha256": uam_hash,
                  "offset": args.offset, "results": results,
                  "limitations": "No GPU execution, vertex binding hints or game-boot proof. "
                                  "UAM host compilation occurs only when uam_sha256 is present."}
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(collections.Counter(row["status"] for row in results))))
    expected = "compiled" if uam else "translated"
    return 0 if all(row["status"] == expected for row in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
