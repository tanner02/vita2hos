import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("title_profile", Path(__file__).parents[2] / "tools/package-title-profile.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def fixture():
    meta = bytearray(0x300)
    meta[:4] = b"META"
    struct.pack_into("<IIII", meta, 0x70, 0x2c0, 0x40, 0x80, 0x240)
    meta[0x2c0:0x2c4] = b"ACI0"
    meta[0x280:0x284] = b"ACID"
    for offset in (0x2d0, 0x290, 0x298): struct.pack_into("<Q", meta, offset, 0x010000000000100d)
    main = b"NSO0" + bytes(252)
    names = b"main\0main.npdm\0"
    return (struct.pack("<4sIII", b"PFS0", 2, len(names), 0) +
        struct.pack("<QQII", 0, len(main), 0, 0) +
        struct.pack("<QQII", len(main), len(meta), 5, 0) + names + main + meta)


class TitleProfileTests(unittest.TestCase):
    def test_retarget_preserves_everything_except_ids(self):
        source = fixture()
        patched = tool.retarget(source, 0x0100152000022000)
        start, end = tool.pfs_files(source)["main.npdm"]
        fields = (0x2d0, 0x290, 0x298)
        allowed = {start + offset + byte for offset in fields for byte in range(8)}
        self.assertTrue(all(a == b or i in allowed for i, (a, b) in enumerate(zip(source, patched))))
        self.assertEqual(len(source), len(patched))
        for offset in fields: self.assertEqual(struct.unpack_from("<Q", patched, start + offset)[0], 0x0100152000022000)
        self.assertEqual(patched[start + 12], 0)

    def test_reject_bad_metadata(self):
        source = bytearray(fixture())
        start, end = tool.pfs_files(source)["main.npdm"]
        for offset, value in ((12, 1), (0x80, 1), (0x280, 0)):
            bad = source.copy(); bad[start + offset] = value
            with self.assertRaises(ValueError): tool.retarget(bad, 0x0100152000022000)

    def test_reject_bad_container_ranges(self):
        for offset, value in ((16, (1 << 64) - 1), (40, 0)):
            bad = bytearray(fixture()); struct.pack_into("<Q", bad, offset, value)
            with self.assertRaises(ValueError): tool.pfs_files(bad)
        with self.assertRaises(ValueError): tool.pfs_files(fixture()[:-1])

    def test_only_application_ids(self):
        for value in ("../exefs", "010000000000100d", "ffffffffffffffff", "0100152000022000/", "-100152000022000"):
            with self.assertRaises(ValueError): tool.program_id(value)
        self.assertEqual(tool.program_id("0100152000022000"), 0x0100152000022000)

    def test_bundle_profile_has_no_global_override(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory) / "smoke.zip"
            runtime = fixture()
            manifest = {"runtime_commit": "c" * 40, "files": {
                "sd/vita2hos/vita2hos.nsp": hashlib.sha256(runtime).hexdigest()}}
            def write_bundle():
                with zipfile.ZipFile(bundle, "w") as archive:
                    archive.writestr("manifest.json", json.dumps(manifest))
                    archive.writestr("sd/vita2hos/vita2hos.nsp", runtime)
            write_bundle()
            files = tool.build_profile(bundle, "0100152000022000")
            self.assertEqual(set(files), {"sd/atmosphere/contents/0100152000022000/exefs.nsp", "manifest.json", "TITLE-PROFILE.txt"})
            output_manifest = json.loads(files["manifest.json"])
            self.assertEqual(output_manifest["hardware_execution"], "not performed")
            for name, checksum in output_manifest["files"].items():
                self.assertEqual(checksum, hashlib.sha256(files[name]).hexdigest())
            manifest["files"]["sd/vita2hos/vita2hos.nsp"] = "0" * 64
            write_bundle()
            with self.assertRaises(ValueError): tool.build_profile(bundle, "0100152000022000")


if __name__ == "__main__": unittest.main()
