"""Check collected owned-probe evidence; never interpret absent logs as a pass."""
import argparse
import json
from pathlib import Path
import re


def verify(report, log, runtime):
    if not re.fullmatch(r"[0-9a-f]{7,40}", runtime):
        raise ValueError("Expected a source commit identifier")
    if not re.search(r"^vita2hos .*?-" + re.escape(runtime) + r"(?: |\n)", log, re.M):
        raise ValueError("Runtime version mismatch")
    if report.splitlines() != ["missing_import_probe v1",
            "PASS repeated_arm_thumb_error_and_r1", "PASS missing_import_probe"]:
        raise ValueError("Incomplete or failing guest report")
    calls = re.findall(r"UNSUPPORTED_CALL kind=function library=(0x[0-9a-f]{8}) "
        r"nid=(0x[0-9a-f]{8}) stub=(0x[0-9a-f]{8}) return_address=(0x[0-9a-f]{8})\b", log)
    expected = {(0x6f776e74, 0xaabb1011), (0x6f776e74, 0xaabb1022)}
    if len(calls) != 2 or {(int(a, 16), int(b, 16)) for a, b, _, _ in calls} != expected:
        raise ValueError("Expected exactly one first-use record for each owned import")
    if any(not int(stub, 16) or not int(address, 16) for _, _, stub, address in calls):
        raise ValueError("Missing call-site addresses")
    for marker in ["UNRESOLVED_TRACE_OVERFLOW", "LIBRARY_STARTUP_ONLY", "FAIL "]:
        if marker in log:
            raise ValueError("Unexpected diagnostic: " + marker)
    for marker in ["Process finished! Returned: 0", "Returned from launch with result: 0",
                   "load_process_close() returned: 0x0"]:
        if marker not in log:
            raise ValueError("Missing clean teardown marker: " + marker)
    return {"probe": "missing_import_probe", "runtime": runtime,
            "result": "PASS", "first_use_records": len(calls)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("log", type=Path)
    parser.add_argument("--runtime", required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.report.read_text(encoding="utf-8"),
        args.log.read_text(encoding="utf-8"), args.runtime), indent=2))
