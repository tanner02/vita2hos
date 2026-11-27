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
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("translator", type=Path)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=32)
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=15)
    args = parser.parse_args()
    if not 1 <= args.limit <= 1024 or args.offset < 0 or not 0 < args.timeout <= 120:
        parser.error("require limit 1..1024, nonnegative offset and timeout 0..120 seconds")
    translator = args.translator.resolve(strict=True)
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
                run = subprocess.run([str(translator), "--single", str(path.resolve())],
                                     capture_output=True, timeout=args.timeout, preexec_fn=limits)
                row.update(returncode=run.returncode,
                           stdout=run.stdout.decode("utf-8", "replace"),
                           stderr=run.stderr.decode("utf-8", "replace"))
                marker = any(line.startswith("TRANSLATED stage=")
                             for line in row["stdout"].splitlines())
                row["status"] = "translated" if run.returncode == 0 and marker else "failed"
            except subprocess.TimeoutExpired:
                row["status"] = "timeout"
            except OSError as error:
                row.update(status="launch_error", stderr=str(error))
        results.append(row)
        # Checkpoint after every program, including failures and timeouts.
        args.output.parent.mkdir(parents=True, exist_ok=True)
        report = {"translator_sha256": translator_hash,
                  "offset": args.offset, "results": results,
                  "limitations": "No GPU execution, vertex binding hints, UAM compilation or game-boot proof."}
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(collections.Counter(row["status"] for row in results))))
    return 0 if all(row["status"] == "translated" for row in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
