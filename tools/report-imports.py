"""Summarize unresolved imports from a vita2hos runtime log (no executable input)."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

RECORD = re.compile(
    r"UNRESOLVED_IMPORT kind=(function|variable) library=(0x[0-9a-fA-F]{8}) "
    r"nid=(0x[0-9a-fA-F]{8})\b")


def sdk_symbols(directory):
    """Read the fixed indentation of the pinned vita-headers v2 database.

    This deliberately accepts only its library nid/functions/variables fields,
    rather than treating arbitrary YAML as executable or requiring PyYAML.
    Keep symbol keys library-scoped: NIDs alone are not globally unique.
    """
    libraries, symbols = {}, {}
    for path in sorted(directory.glob("*.yml")):
        library_name = library = kind = None
        for line in path.read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"      ([A-Za-z0-9_]+):", line)
            if match:
                library_name, library, kind = match[1], None, None
            match = re.fullmatch(r"        nid: (0x[0-9a-fA-F]+)", line)
            if match and library_name:
                library = int(match[1], 16)
                libraries[library] = library_name
            match = re.fullmatch(r"        (functions|variables):", line)
            if match:
                kind = "function" if match[1] == "functions" else "variable"
            match = re.fullmatch(r"          ([A-Za-z0-9_]+): (0x[0-9a-fA-F]+)", line)
            if match and library is not None and kind:
                symbols[(library, kind, int(match[2], 16))] = match[1]
    return libraries, symbols


def report(log, libraries, symbols):
    records = Counter((int(m[2], 16), m[1], int(m[3], 16))
                      for m in RECORD.finditer(log))
    return [{"library_nid": f"0x{library:08x}",
             "library": libraries.get(library), "kind": kind,
             "nid": f"0x{nid:08x}", "symbol": symbols.get((library, kind, nid)),
             "log_occurrences": occurrences}
            for (library, kind, nid), occurrences in sorted(records.items())]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--sdk-db", type=Path, default=Path(__file__).resolve().parents[1] /
                        "external/vita-headers/db/360")
    args = parser.parse_args()
    if not args.sdk_db.is_dir():
        parser.error("SDK database missing; initialize external/vita-headers or pass --sdk-db")
    libraries, symbols = sdk_symbols(args.sdk_db)
    rows = report(args.log.read_text(encoding="utf-8", errors="replace"), libraries, symbols)
    print(json.dumps({"unresolved_imports": rows, "unique_imports": len(rows),
                      "note": "Only imports observed in this log; UDP loss or a partial run can "
                              "omit records. Occurrences are log records, not function calls."},
                     indent=2))


if __name__ == "__main__":
    main()
