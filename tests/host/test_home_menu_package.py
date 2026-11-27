"""Offline standalone title packaging validation; no console keys or tools."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

source = Path(__file__).resolve().parents[2] / "tools/package-home-menu.py"
spec = importlib.util.spec_from_file_location("home_menu", source)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def fixture():
    payloads = [bytes([i]) * 0xc00 for i in range(3)]
    names = [hashlib.sha256(data).hexdigest()[:32] + (".cnmt.nca" if i == 0 else ".nca")
             for i, data in enumerate(payloads)]
    strings = b"".join(name.encode() + b"\0" for name in names)
    table, offset, name_offset = bytearray(), 0, 0
    for name, data in zip(names, payloads):
        table += struct.pack("<QQII", offset, len(data), name_offset, 0)
        offset += len(data)
        name_offset += len(name) + 1
    return struct.pack("<4sIII", b"PFS0", 3, len(strings), 0) + table + strings + b"".join(payloads)


class PackageTests(unittest.TestCase):
    def test_project_icon_dimensions_and_bad_frames(self):
        icon = (source.parents[1] / "assets/home-menu-icon.jpg").read_bytes()
        tool.validate_icon(icon)
        for bad in (b"", b"\xff\xd8\xff\xd9", icon[:30]):
            with self.assertRaises(ValueError): tool.validate_icon(bad)
        marker = icon.index(b"\xff\xc0")
        bad = bytearray(icon)
        struct.pack_into(">H", bad, marker + 5, 128)
        with self.assertRaises(ValueError): tool.validate_icon(bad)

    def test_three_hashed_content_archives(self):
        self.assertEqual(len(tool.installed_contents(fixture())), 3)

    def test_changed_content_and_malformed_tables_refused(self):
        original = fixture()
        bad = bytearray(original); bad[-1] ^= 1
        cases = [bad, b"PFS0", original[:-1]]
        bad = bytearray(original); struct.pack_into("<Q", bad, 16, len(original))
        cases.append(bad)
        bad = bytearray(original); struct.pack_into("<Q", bad, 40, 0)
        cases.append(bad)
        for data in cases:
            with self.assertRaises(ValueError): tool.installed_contents(data)

    def test_application_id_alignment(self):
        self.assertEqual(tool.title_id(tool.DEFAULT_PROGRAM_ID), 0x0100f27432680000)
        for value in ("010000000000100d", "0100152000022001", "../../x", "0" * 16):
            with self.assertRaises(ValueError): tool.title_id(value)

    def test_key_subset_excludes_unrelated_definitions(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "input", Path(directory) / "filtered"
            # Synthetic all-zero data, never actual console material.
            source.write_text("header_key = " + "0" * 64 + "\nkey_area_key_application_00 = " +
                              "0" * 32 + "\nunrelated_new_format = ignored\n")
            tool.packer_keyset(source, output)
            self.assertEqual(len(output.read_text().splitlines()), 2)
            self.assertNotIn("unrelated", output.read_text())
            source.write_text("header_key = broken\n")
            with self.assertRaises(ValueError): tool.packer_keyset(source, Path(directory) / "bad")


if __name__ == "__main__": unittest.main()
