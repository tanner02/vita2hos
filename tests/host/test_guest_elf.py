import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("guest_elf", Path(__file__).parents[2] / "tools/guest_elf.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def guest_fixture():
    data = bytearray(0x200)
    data[:6] = b"\x7fELF\x01\x01"
    struct.pack_into("<HH", data, 16, 0xfe04, 40)
    struct.pack_into("<II", data, 24, 0x80, 52)
    struct.pack_into("<HH", data, 42, 32, 1)
    struct.pack_into("<8I", data, 52, 1, 0x80, 0x81000000, 0, 0x180, 0x180, 5, 0x1000)
    struct.pack_into("<II", data, 0x100 + 0x2c, 0xe0, 0x114)
    struct.pack_into("<I", data, 0x100 + 0x44, 0x11)
    struct.pack_into("<4H", data, 0x160, 0x34, 1, 0, 3)
    return data


class GuestTests(unittest.TestCase):
    def test_fixed_executable_requires_explicit_opt_in(self):
        data = guest_fixture()
        struct.pack_into("<H", data, 16, 0xfe00)
        with self.assertRaises(ValueError): tool.validate_guest(data)
        self.assertEqual(tool.validate_guest(data, allow_fixed=True)["function_imports"], 3)

    def test_entry_and_import_count(self):
        self.assertEqual(tool.validate_guest(guest_fixture()),
                         {"entry_segment": 0, "entry_offset": 0x11, "function_imports": 3})

    def test_image_module_without_entry_is_rejected(self):
        data = guest_fixture()
        struct.pack_into("<I", data, 0x144, 0xffffffff)
        with self.assertRaisesRegex(ValueError, "entry point"): tool.validate_guest(data)

    def test_bad_header_segment_ranges_and_imports(self):
        for offset, fmt, value in ((4, "B", 2), (16, "H", 2), (18, "H", 183),
                (24, "I", 0x40000080), (28, "I", 0xffffffff), (42, "H", 0),
                (52, "I", 0), (52 + 24, "I", 4), (52 + 16, "I", 0x1000),
                (0x12c, "I", 0), (0x130, "I", 0xe0), (0x130, "I", 0x1000),
                (0x144, "I", 0x181), (0x160, "H", 0), (0x166, "H", 0)):
            with self.subTest(offset=offset, value=value):
                data = guest_fixture()
                struct.pack_into("<" + fmt, data, offset, value)
                with self.assertRaises(ValueError): tool.validate_guest(data)
        with self.assertRaises(ValueError): tool.validate_guest(guest_fixture()[:30])


if __name__ == "__main__": unittest.main()
