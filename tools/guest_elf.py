"""Structural checks for the freestanding Vita hardware-test executables."""
import struct


def validate_guest(data, allow_fixed=False):
    types = (0xfe04, 0xfe00) if allow_fixed else (0xfe04,)
    if (len(data) < 52 or data[:6] != b"\x7fELF\x01\x01" or
        struct.unpack_from("<H", data, 16)[0] not in types or struct.unpack_from("<H", data, 18)[0] != 40):
        raise ValueError("Expected an ELF32 ARM Vita executable")
    entry, phoff = struct.unpack_from("<II", data, 24)
    phsize, phcount = struct.unpack_from("<HH", data, 42)
    index, relative = entry >> 30, entry & 0x3fffffff
    if phsize != 32 or index >= phcount or phoff + phsize * phcount > len(data):
        raise ValueError("Invalid module-info program header")
    kind, offset, _, _, size, memory, flags, _ = struct.unpack_from("<8I", data, phoff + index * phsize)
    if kind != 1 or not flags & 1 or size > memory or offset + size > len(data) or relative + 0x5c > size:
        raise ValueError("Module info must reside in a file-backed executable segment")
    info = offset + relative
    first, last = struct.unpack_from("<II", data, info + 0x2c)
    start = struct.unpack_from("<I", data, info + 0x44)[0]
    if start == 0xffffffff or (start & ~1) + (2 if start & 1 else 4) > size:
        raise ValueError("Guest has no valid module entry point; check imagemodule configuration")
    if first % 4 or last % 4 or first >= last or last > size:
        raise ValueError("Guest has no valid runtime import range")
    count = 0
    while first < last:
        if last - first < 2: raise ValueError("Truncated import record")
        record_size = struct.unpack_from("<H", data, offset + first)[0]
        if record_size not in (0x24, 0x34) or record_size > last - first:
            raise ValueError("Invalid generated runtime import record")
        count += struct.unpack_from("<H", data, offset + first + 6)[0]
        first += record_size
    if not count: raise ValueError("Hardware probe has no function imports")
    return {"entry_segment": index, "entry_offset": start, "function_imports": count}
