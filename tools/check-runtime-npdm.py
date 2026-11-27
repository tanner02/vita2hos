"""Validate generated ARM32 runtime metadata before publishing a build artifact."""
import argparse
import importlib.util
from pathlib import Path
import struct

spec = importlib.util.spec_from_file_location("runtime_metadata", Path(__file__).with_name("package-title-profile.py"))
metadata = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metadata)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("runtime", type=Path, help="Runtime ExeFS NSP")
args = parser.parse_args()
data = args.runtime.read_bytes()
start, end = metadata.pfs_files(data)["main.npdm"]
meta = data[start:end]
if len(meta) < 0x80: raise ValueError("Truncated NPDM")
aci = struct.unpack_from("<I", meta, 0x70)[0]
if aci + 0x18 > len(meta): raise ValueError("Truncated ACI0")
program = struct.unpack_from("<Q", meta, aci + 0x10)[0]
if metadata.retarget(data, program) != data:
    raise ValueError("Runtime program IDs are inconsistent")
print("Validated ARM32 NPDM regions, program IDs and mutually exclusive debug capabilities.")
