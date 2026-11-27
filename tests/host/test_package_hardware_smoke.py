"""Offline packaging checks with synthetic artifact/sample archives."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile
from test_guest_elf import guest_fixture

source = Path(__file__).resolve().parents[2] / "tools/package-hardware-smoke.py"
spec = importlib.util.spec_from_file_location("package_smoke", source)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def archive_bytes(files):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as archive:
        for name, data in files.items(): archive.writestr(name, data)
    return buffer.getvalue()


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.artifact = self.root / "runtime.zip"
        self.artifact.write_bytes(archive_bytes({"vita2hos.nsp": b"PFS0runtime"}))
        self.cube = b"\x7fELFcube-fixture"
        sample = archive_bytes({"gxm_cube_sample.velf": self.cube})
        (self.root / "gxm_cube_sample.zip").write_bytes(sample)
        self.expected = hashlib.sha256(sample).hexdigest()
        self.probe = guest_fixture()
        self.output = self.root / "bundle.zip"

    def package(self, probe=None, draw=False):
        args = ["package", str(self.artifact), "--commit", "a" * 40, "--build-run", "offline-test",
                "--output", str(self.output), "--samples-dir", str(self.root)]
        if probe is not None:
            path = self.root / "probe"
            path.write_bytes(probe)
            args += ["--draw-probe" if draw else "--surface-probe", str(path)]
        with patch("sys.argv", args), patch.object(package, "SAMPLES", {"gxm_cube_sample": self.expected}):
            package.main()

    def test_optional_probe_preserves_default_cube_and_hashes(self):
        self.package(self.probe)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(archive.read("sd/vita2hos/executable"), self.cube)
            self.assertEqual(archive.read("samples/surface_probe/executable"), self.probe)
            manifest = json.loads(archive.read("manifest.json"))
            self.assertEqual(manifest["hardware_execution"], "not performed")
            for name, digest in manifest["files"].items():
                self.assertEqual(hashlib.sha256(archive.read(name)).hexdigest(), digest)
            self.assertFalse(any("override_config.ini" in name for name in archive.namelist()))

    def test_probe_remains_optional(self):
        self.package()
        with zipfile.ZipFile(self.output) as archive:
            self.assertNotIn("samples/surface_probe/executable", archive.namelist())

    def test_draw_probe_includes_fixture_license(self):
        self.package(self.probe, draw=True)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(archive.read("samples/draw_probe/executable"), self.probe)
            self.assertIn(b"GNU GENERAL PUBLIC LICENSE", archive.read("DRAW-PROBE-COPYING.txt"))

    def test_invalid_probe_architecture_or_container_refused(self):
        cases = [b"", b"PFS0" + bytes(52)]
        for offset in (4, 5, 16, 18):
            bad = self.probe.copy()
            bad[offset] ^= 1
            cases.append(bad)
        for data in cases:
            with self.subTest(header=data[:20]):
                with self.assertRaises(ValueError): self.package(data)
                self.assertFalse(self.output.exists())


if __name__ == "__main__": unittest.main()
