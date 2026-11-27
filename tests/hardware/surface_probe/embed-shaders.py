"""Embed pinned repository GXP fixtures; does not compile or execute shaders."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = ["/* Generated from pinned Vita3K native-tool test fixtures (GPL-2.0-or-later). */", "#include <stdint.h>"]
for name in ("color_v", "clear_f", "texture_v", "texture_f"):
    data = (source / (name + ".gxp")).read_bytes()
    if data[:4] != b"GXP\0": raise ValueError("Invalid GXP fixture: " + name)
    text.append("static const uint8_t " + name + "[] __attribute__((aligned(4))) = {")
    text += [",".join(f"0x{byte:02x}" for byte in data[i:i + 16]) + "," for i in range(0, len(data), 16)]
    text.append("};")
output.write_text("\n".join(text) + "\n", encoding="ascii")
