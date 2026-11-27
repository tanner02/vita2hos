import importlib.util
from pathlib import Path
import struct
import unittest
import zlib
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("tp_plan", ROOT / "tools/make-tls-plan.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def image():
    data = bytearray(112)
    data[:6] = b"\x7fELF\x01\x01"
    struct.pack_into("<HHI", data, 16, 0xfe04, 40, 1)
    struct.pack_into("<I", data, 28, 52)
    struct.pack_into("<HH", data, 42, 32, 1)
    struct.pack_into("<8I", data, 52, 1, 96, 0x81000000, 0, 16, 16, 5, 4)
    struct.pack_into("<I", data, 96, 0xee1d0f70)
    struct.pack_into("<I", data, 104, 0x0f70ee1d)
    return data


class Plans(unittest.TestCase):
    def test_cli_protects_source(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "probe.elf"
            original = bytes(image())
            source.write_bytes(original)
            command = [sys.executable, str(ROOT / "tools/make-tls-plan.py"), str(source),
                       "--site", "0:0:arm", "--output"]
            result = subprocess.run(command + [str(source)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), original)
            alias = Path(directory) / "hardlink.tlsplan"
            os.link(source, alias)
            result = subprocess.run(command + [str(alias)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), original)

    def test_encoding_and_fingerprint(self):
        data = image()
        plan = tool.make_plan(data, [(0, 0, "arm"), (0, 8, "thumb")])
        self.assertEqual(struct.unpack_from("<8sIIQQ", plan),
                         (b"V2HOSTP1", 1, 2, tool.source_tag(data), len(data)))
        self.assertEqual(struct.unpack_from("<4I", plan, 48), (0, 8, 0x0f70ee1d, 1))
        self.assertEqual(tool.source_tag(b"hello"), 0xa430d84680aabd0b)
        data[-1] ^= 1
        self.assertNotEqual(struct.unpack_from("<Q", plan, 16)[0], tool.source_tag(data))

    def test_bad_sites(self):
        for sites in ([], [(1, 0, "arm")], [(0, 1, "arm")], [(0, 12, "arm")],
                      [(0, 0, "bad")], [(0, 0, "arm")] * 2):
            with self.assertRaises(ValueError):
                tool.make_plan(image(), sites)
        data = image()
        struct.pack_into("<I", data, 96, 0xee1ddf70)  # Unsupported SP destination.
        with self.assertRaises(ValueError):
            tool.make_plan(data, [(0, 0, "arm")])

    def test_compressed_self(self):
        elf = image()
        data = bytearray(256 + len(elf))
        data[:4] = b"SCE\0"
        struct.pack_into("<Q", data, 16, 256)
        struct.pack_into("<4Q", data, 64, 256, 308, 0, 128)
        content = zlib.compress(elf[96:112])
        struct.pack_into("<4Q", data, 128, len(data), len(content), 2, 2)
        data[256:] = elf
        data.extend(content)
        self.assertEqual(len(tool.make_plan(data, [(0, 8, "thumb")])), 48)
        data[-1] ^= 1
        with self.assertRaises((ValueError, zlib.error)):
            tool.make_plan(data, [(0, 8, "thumb")])

    def test_plain_self(self):
        elf = image()
        data = bytearray(256 + len(elf))
        data[:4] = b"SCE\0"
        struct.pack_into("<Q", data, 16, 256)
        struct.pack_into("<4Q", data, 64, 256, 256 + 52, 0, 128)
        struct.pack_into("<4Q", data, 128, 0, 0, 1, 2)
        data[256:] = elf
        self.assertEqual(len(tool.make_plan(data, [(0, 0, "arm")])), 48)
        struct.pack_into("<Q", data, 152, 1)  # Encrypted: reject, never decrypt.
        with self.assertRaises(ValueError):
            tool.make_plan(data, [(0, 0, "arm")])


if __name__ == "__main__":
    unittest.main()
