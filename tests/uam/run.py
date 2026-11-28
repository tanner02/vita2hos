#!/usr/bin/env python3
"""Run the original clone regression against an existing Linux Meson UAM build."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True,
                        help="Completed host UAM Meson build with compile_commands.json")
    args = parser.parse_args()
    build = args.build.resolve()
    entries = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in entries
                 if Path(item["file"]).name == "nv50_ir.cpp")
    command = shlex.split(entry["command"])
    # Reuse the real compiler's generated-header includes and configuration.
    # Redirect dependency output too, so this check never changes build objects.
    with tempfile.TemporaryDirectory(prefix="uam-clone-check-") as directory:
        temporary = Path(directory)
        obj = temporary / "clone.o"
        for flag, value in [("-o", obj), ("-MF", temporary / "clone.d"), ("-MQ", obj)]:
            if flag in command:
                command[command.index(flag) + 1] = str(value)
        command[-1] = str(Path(__file__).with_name("clone.cpp").resolve())
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
        executable = temporary / "clone-check"
        link[link.index("-o") + 1] = str(executable)
        subprocess.run(link, cwd=build, check=True)
        result = subprocess.run([str(executable)], check=False, capture_output=True, text=True)
        print(result.stdout, end="")
        print(result.stderr, end="")
        print(f"Clone regression exit status: {result.returncode}")
        return 0 if result.returncode == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
