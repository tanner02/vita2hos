"""Summarize unresolved imports from a vita2hos runtime log (no executable input)."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

RECORD = re.compile(
    r"UNRESOLVED_IMPORT kind=(function|variable) library=(0x[0-9a-fA-F]{8}) "
    r"nid=(0x[0-9a-fA-F]{8})\b")
LIBRARY = re.compile(r"Resolving imports for library NID (0x[0-9a-fA-F]{8})\b")
LEGACY_FUNCTION = re.compile(r"Could not resolve NID (0x[0-9a-fA-F]{8}), export not found!")
LEGACY_VARIABLE = re.compile(r"Trying to resolve variable NID (0x[0-9a-fA-F]{8})\b")


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
    structured = Counter((int(m[2], 16), m[1], int(m[3], 16))
                         for m in RECORD.finditer(log))
    legacy = Counter()
    library = None
    for line in log.splitlines():
        match = LIBRARY.search(line)
        if match:
            library = int(match[1], 16)
        if library is None:
            continue  # Never guess a library for an orphaned NID.
        for kind, pattern in (("function", LEGACY_FUNCTION), ("variable", LEGACY_VARIABLE)):
            match = pattern.search(line)
            if match:
                legacy[(library, kind, int(match[1], 16))] += 1
    # Current builds emit both representations. Prefer explicit library-scoped
    # records per symbol; legacy-only symbols remain useful after UDP loss.
    records = legacy.copy()
    for key, occurrences in structured.items():
        records[key] = occurrences
    return [{"library_nid": f"0x{library:08x}",
             "library": libraries.get(library), "kind": kind,
             "nid": f"0x{nid:08x}", "symbol": symbols.get((library, kind, nid)),
             "evidence": "structured" if (library, kind, nid) in structured else "legacy_context",
             "log_occurrences": occurrences}
            for (library, kind, nid), occurrences in sorted(records.items())]


CALL = re.compile(
    r"UNSUPPORTED_CALL kind=function library=(0x[0-9a-fA-F]{8}) "
    r"nid=(0x[0-9a-fA-F]{8}) stub=(0x[0-9a-fA-F]{8}) "
    r"return_address=(0x[0-9a-fA-F]{8,16})\b")


def called_imports(log, libraries, symbols):
    """First-use reports per import site; repeated log records are not calls."""
    sites = {}
    for match in CALL.finditer(log):
        library, nid, stub, address = (int(value, 16) for value in match.groups())
        key = library, nid, stub
        if key not in sites:
            sites[key] = {"library_nid": f"0x{library:08x}",
                          "library": libraries.get(library), "kind": "function",
                          "nid": f"0x{nid:08x}", "symbol": symbols.get((library, "function", nid)),
                          "stub_address": f"0x{stub:08x}",
                          "first_return_address": f"0x{address:08x}",
                          "evidence": "runtime_first_use", "log_occurrences": 0}
        sites[key]["log_occurrences"] += 1
    return [sites[key] for key in sorted(sites)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--sdk-db", type=Path, default=Path(__file__).resolve().parents[1] /
                        "external/vita-headers/db/360")
    args = parser.parse_args()
    if not args.sdk_db.is_dir():
        parser.error("SDK database missing; initialize external/vita-headers or pass --sdk-db")
    libraries, symbols = sdk_symbols(args.sdk_db)
    log = args.log.read_text(encoding="utf-8", errors="replace")
    rows = report(log, libraries, symbols)
    calls = called_imports(log, libraries, symbols)
    print(json.dumps({"unresolved_imports": rows, "unique_imports": len(rows),
                      "first_unsupported_calls": calls,
                      "first_unsupported_call_sites": len(calls),
                      "note": "Only imports observed in this log; UDP loss or a partial run can "
                              "omit records. Legacy library context depends on log ordering. "
                              "Occurrences are log records, not function calls. Runtime reports record only the first use per traced import site/lifetime; missing records do not prove a function was unused."},
                     indent=2))


if __name__ == "__main__":
    main()
