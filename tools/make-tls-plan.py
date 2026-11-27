#!/usr/bin/env python3
"""Encode explicitly reviewed TPIDRURO instruction sites; never discover sites."""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib


def span(data, offset, size):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError("Truncated image range")
    return data[offset:offset + size]


def source_tag(data):
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & 0xffffffffffffffff
    return result


def segments(data):
    self_image = data[:4] == b"SCE\0"
    if self_image:
        span(data, 0, 0x60)
        base = struct.unpack_from("<Q", data, 0x10)[0]
        elf_offset, phoff, _, info_offset = struct.unpack_from("<4Q", data, 0x40)
        elf = span(data, elf_offset, 52)
    else:
        elf = span(data, 0, 52)
        phoff = struct.unpack_from("<I", elf, 28)[0]
    if (elf[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", elf, 18)[0] != 40 or
            struct.unpack_from("<H", elf, 16)[0] not in (0xfe00, 0xfe04)):
        raise ValueError("Expected ELF32 ARM Vita ELF or plaintext SELF")
    entry_size, count = struct.unpack_from("<HH", elf, 42)
    if entry_size != 32 or not 0 < count <= 64:
        raise ValueError("Invalid program-header layout")
    headers = span(data, phoff, count * 32)
    if self_image:
        span(data, info_offset, count * 32)
    result = {}
    for index in range(count):
        kind, offset, address, _, size, memory, flags, _ = struct.unpack_from("<8I", headers, index * 32)
        if kind != 1:
            continue
        if size > memory:
            raise ValueError("Segment file size exceeds memory size")
        if self_image:
            stored, length, compression, encryption = struct.unpack_from("<4Q", data, info_offset + index * 32)
            if encryption != 2:
                raise ValueError("SELF segment is encrypted; no decryption is performed")
            if compression == 1:
                content = span(data, base + offset, size)
            elif compression == 2:
                decoder = zlib.decompressobj()
                content = decoder.decompress(span(data, stored, length), size + 1)
                if len(content) != size or not decoder.eof or decoder.unconsumed_tail:
                    raise ValueError("Invalid compressed segment")
            else:
                raise ValueError("Unsupported SELF segment compression")
        else:
            content = span(data, offset, size)
        result[index] = address, flags, content
    return result


def make_plan(data, sites):
    if not 0 < len(sites) <= 1024:
        raise ValueError("Expected 1..1024 explicitly reviewed sites")
    loaded = segments(data)
    records = []
    for index, offset, mode in sites:
        if index not in loaded or mode not in ("arm", "thumb"):
            raise ValueError("Invalid segment or instruction mode")
        address, flags, content = loaded[index]
        if (not flags & 1 or address + offset > 0xffffffff - 3 or
                (address + offset) % (4 if mode == "arm" else 2)):
            raise ValueError("Site is not aligned executable code")
        word = struct.unpack("<I", span(content, offset, 4))[0]
        if mode == "arm":
            valid = word & 0x0fff0fff == 0x0e1d0f70 and word >> 28 != 15
            register = word >> 12 & 15
        else:
            valid = word & 0x0fffffff == 0x0f70ee1d
            register = word >> 28
        if not valid or register in (13, 15):
            raise ValueError("Site does not encode a supported TPIDRURO read")
        for previous, start, _, _ in records:
            if previous == index and offset < start + 4 and start < offset + 4:
                raise ValueError("Overlapping patch sites")
        records.append((index, offset, word, 0 if mode == "arm" else 1))
    return (struct.pack("<8sIIQQ", b"V2HOSTP1", 1, len(records), source_tag(data), len(data)) +
            b"".join(struct.pack("<4I", *record) for record in records))


def parse_site(value):
    try:
        segment, offset, mode = value.split(":")
        return int(segment, 0), int(offset, 0), mode
    except ValueError as error:
        raise argparse.ArgumentTypeError("Use SEGMENT:OFFSET:arm|thumb") from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--site", action="append", type=parse_site, required=True,
                        help="Independently reviewed instruction boundary: SEGMENT:OFFSET:arm|thumb")
    parser.add_argument("--output", type=Path, help="Default: IMAGE.tlsplan")
    args = parser.parse_args()
    try:
        data = args.image.read_bytes()
        plan = make_plan(data, args.site)
        target = args.output or Path(str(args.image) + ".tlsplan")
        if target.resolve() == args.image.resolve() or (target.exists() and target.samefile(args.image)):
            raise ValueError("Output must not replace the source image")
        target.write_bytes(plan)
    except (OSError, ValueError, zlib.error) as error:
        parser.error(str(error))
    print(f"Created {target}: {len(args.site)} explicit sites; source SHA-256 {hashlib.sha256(data).hexdigest()}")
    print("Experimental: instruction boundaries require independent review; Horizon behavior is unverified.")


if __name__ == "__main__":
    main()
