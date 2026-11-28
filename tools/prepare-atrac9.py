"""Prepare narrow integer fixes for the pinned ATRAC9 C backend without editing it."""
import argparse
import hashlib
import json
from pathlib import Path


def prepare(source, output):
    output.mkdir(parents=True, exist_ok=True)
    utility = (source / 'utility.c').read_text(encoding='utf-8')
    reverse = 'unsigned int BitReverse32(unsigned int value, int bitCount)\n{'
    assert utility.count(reverse) == 1, 'Pinned bit-reversal source changed'
    utility = utility.replace(reverse, reverse + '\n\tif (bitCount <= 0 || bitCount > 32) return 0;')
    old = '''\tconst int shift = 8 * sizeof(int) - bits;
\treturn (value << shift) >> shift;'''
    assert utility.count(old) == 1, 'Pinned sign-extension source changed'
    utility = utility.replace(old, '''\tif (bits <= 0) return 0;
\tif (bits >= 32) return value;
\tconst unsigned int sign = 1U << (bits - 1);
\tconst unsigned int masked = (unsigned int)value & ((1U << bits) - 1U);
\treturn (int)(masked ^ sign) - (int)sign;''')
    reader = (source / 'bit_reader.c').read_text(encoding='utf-8')
    old = 'int value = buffer[byteIndex] << 24'
    assert reader.count(old) == 1, 'Pinned wide bit-reader source changed'
    reader = reader.replace(old, 'unsigned int value = (unsigned int)buffer[byteIndex] << 24')
    old = '\tint value = 0;'
    assert reader.count(old) == 1, 'Pinned fallback bit-reader source changed'
    reader = reader.replace(old, '\tunsigned int value = 0;')
    allocation = (source / 'bit_allocation.c').read_text(encoding='utf-8')
    # The syntax permits a gradient endpoint beyond the coded band. Only
    # quantization units actually consumed by CalculatePrecisions need storage;
    # preserve the original curve's unitCount and interpolation indexes.
    old = 'i < block->GradientEndUnit; i++'
    assert allocation.count(old) == 3, 'Pinned gradient loops changed'
    allocation = allocation.replace(old,
        'i < block->GradientEndUnit && i <= block->QuantizationUnitCount; i++')
    records = {}
    for name, data in [('utility.c', utility), ('bit_reader.c', reader),
                       ('bit_allocation.c', allocation)]:
        blob = data.encode('utf-8')
        target = output / name
        if not target.exists() or target.read_bytes() != blob:
            target.write_bytes(blob)
        records[name] = {'source_sha256': hashlib.sha256((source / name).read_bytes()).hexdigest(),
                         'prepared_sha256': hashlib.sha256(blob).hexdigest()}
    (output / 'compatibility.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source, args.output)
