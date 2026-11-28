#!/usr/bin/env python3
"""Run an original IR fixture against an existing Linux Meson UAM build."""
import argparse
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True,
                        help="Completed host UAM Meson build with compile_commands.json")
    parser.add_argument("--fixture", choices=["clone", "intervals"], default="clone")
    parser.add_argument("--interval-source", type=Path,
                        help="Prepared interval implementation; enables cache-retirement assertions")
    parser.add_argument("--sanitize", action="store_true",
                        help="Instrument the fixture and included interval implementation with ASan/UBSan")
    args = parser.parse_args()
    if args.interval_source and args.fixture != 'intervals':
        parser.error('--interval-source requires --fixture intervals')
    build = args.build.resolve()
    entries = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in entries
                 if Path(item["file"]).name == "nv50_ir.cpp")
    command = shlex.split(entry["command"])
    # Reuse the real compiler's generated-header includes and configuration.
    # Redirect dependency output too, so this check never changes build objects.
    with tempfile.TemporaryDirectory(prefix="uam-clone-check-") as directory:
        temporary = Path(directory)
        obj = temporary / "fixture.o"
        for flag, value in [("-o", obj), ("-MF", temporary / "clone.d"), ("-MQ", obj)]:
            if flag in command:
                command[command.index(flag) + 1] = str(value)
        command[-1] = str(Path(__file__).with_name(args.fixture + ".cpp").resolve())
        util = None
        if args.fixture == "intervals":
            util = next(item for item in entries
                        if Path(item["file"]).name == "nv50_ir_util.cpp")
            implementation = (args.interval_source.resolve() if args.interval_source else
                              (Path(util["directory"]) / util["file"]).resolve())
            command[1:1] = ['-DUAM_INTERVAL_IMPLEMENTATION="' + str(implementation) + '"']
            if args.interval_source:
                preparer_path = Path(__file__).resolve().parents[2] / 'tools/prepare-uam-intervals.py'
                spec = importlib.util.spec_from_file_location('prepare_intervals', preparer_path)
                preparer = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(preparer)
                original = (Path(util['directory']) / util['file']).resolve()
                raw, header = original.read_text(), original.with_suffix('.h').read_text()
                if preparer.prepare(raw, header) != implementation.read_text():
                    raise ValueError('Interval implementation does not match the production preparer')
                for changed in [(raw + '\n', header), (raw, header + '\n')]:
                    try:
                        preparer.prepare(*changed)
                    except ValueError:
                        continue
                    raise ValueError('Changed SDK interval source/header was not rejected')
                command.insert(1, '-DUAM_INTERVAL_INDEXED')
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        subprocess.run(command, cwd=entry["directory"], check=True)
        # Preserve the actual linker's launcher, sanitizer/LTO/runtime flags.
        commands = subprocess.check_output(
            ["ninja", "-C", str(build), "-t", "commands", "uam"], text=True)
        link = shlex.split(commands.splitlines()[-1])
        main = next(item for item in entries
                    if Path(item["file"]).name == "main.cpp"
                    and Path(item["file"]).parent.name == "source")
        main_object = (Path(main["directory"]) / main["output"]).resolve()
        positions = [index for index, value in enumerate(link)
                     if (build / value).resolve() == main_object]
        if len(positions) != 1:
            raise ValueError("UAM link layout changed: expected exactly one CLI main object")
        link[positions[0]] = str(obj)
        if util:
            # The fixture includes the selected implementation so it can verify
            # retirement of its private cache. Do not link a second util object.
            util_object = (Path(util["directory"]) / util["output"]).resolve()
            positions = [i for i, value in enumerate(link)
                         if (build / value).resolve() == util_object]
            if len(positions) != 1:
                raise ValueError("UAM link layout changed: expected exactly one interval object")
            del link[positions[0]]
        if args.sanitize:
            link.insert(1, '-fsanitize=address,undefined')
        executable = temporary / (args.fixture + "-check")
        link[link.index("-o") + 1] = str(executable)
        subprocess.run(link, cwd=build, check=True)
        result = subprocess.run([str(executable)], check=False, capture_output=True, text=True)
        print(result.stdout, end="")
        print(result.stderr, end="")
        print(f"{args.fixture} regression exit status: {result.returncode}")
        return 0 if result.returncode == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
