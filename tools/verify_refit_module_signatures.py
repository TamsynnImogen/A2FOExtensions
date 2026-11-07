#!/usr/bin/env python3
"""Read-only native anchor verification. Does not launch, load or patch a game.

The output proves FILE signature and relocation matches only. Runtime admission
additionally requires a reviewed host identity, verified runtime RVAs and
relocations, and unowned live sites. The PE preferred base is 0x400000.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def inspect(path):
    data = path.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError('not MZ')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('not PE')
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    opt_size = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    if machine != 0x14c or struct.unpack_from('<H', data, opt)[0] != 0x10b:
        raise ValueError('not PE32 i386')
    base = struct.unpack_from('<I', data, opt + 28)[0]
    if base != 0x400000:
        raise ValueError('preferred base differs')
    image_size = struct.unpack_from('<I', data, opt + 56)[0]
    if opt_size < 96 + 8 * 6 or opt + opt_size > len(data):
        raise ValueError('optional header has no relocation directory')
    directory_count = struct.unpack_from('<I', data, opt + 92)[0]
    if directory_count <= 5:
        raise ValueError('relocation directory is absent')
    reloc_rva, reloc_size = struct.unpack_from('<II', data, opt + 96 + 8 * 5)
    if not reloc_rva or not reloc_size or reloc_rva >= image_size or reloc_size > image_size - reloc_rva:
        raise ValueError('relocation directory is outside image')
    sections = []
    for n in range(count):
        off = opt + opt_size + 40 * n
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', data, off + 8)
        sections.append((rva, virtual_size, raw_offset, raw_size))

    def read(rva, size):
        for start, _, raw, raw_size in sections:
            at = rva - start
            if 0 <= at <= raw_size and size <= raw_size - at:
                if raw + at + size > len(data):
                    break
                return data[raw + at:raw + at + size]
        raise ValueError(f'RVA {rva:#x} is not file backed')

    relocations = set()
    directory = read(reloc_rva, reloc_size)
    offset = 0
    while offset < reloc_size:
        # The clean pinned image pads the declared directory with zero bytes.
        # Only a wholly zero terminal tail is padding; malformed data fails.
        if not any(directory[offset:]):
            break
        if reloc_size - offset < 8:
            raise ValueError('truncated relocation block header')
        page, block_size = struct.unpack_from('<II', directory, offset)
        if block_size < 8 or block_size > reloc_size - offset or block_size % 2:
            raise ValueError('invalid relocation block size')
        for at in range(offset + 8, offset + block_size, 2):
            entry = struct.unpack_from('<H', directory, at)[0]
            if entry >> 12 == 3:  # IMAGE_REL_BASED_HIGHLOW
                operand = page + (entry & 0xfff)
                if operand > image_size or 4 > image_size - operand:
                    raise ValueError('HIGHLOW operand is outside image')
                relocations.add(operand)
        offset += block_size

    # The byte patterns are also independently declared in bindings.hpp, so
    # native runtime and file audit must both agree with the pinned inputs.
    claims = [
        ('pan parsed-key context', 0x91766, '68ccd76f00e8006c1c008b45f085c07508d91db4fb7000eb02'),
        ('right drag X consumer', 0x1268d7, 'd80db4fb7000'),
        ('right drag Y consumer', 0x1268f5, 'd80db4fb7000'),
        ('view and restoration copy context', 0x91bef, '8b0dc0ca6f008915fc3c77008d55f0890dc4ca6f00'),
    ]
    checks = []
    for label, rva, text in claims:
        expected = bytes.fromhex(text)
        actual = read(rva, len(expected))
        checks.append(dict(label=label, rva=f'0x{rva:08x}', expected=text,
                           actual=actual.hex(), matches=actual == expected))
    for rva in (0x91767, 0x91779, 0x1268d9, 0x1268f7,
                0x91bf1, 0x91bf7, 0x91c00):
        found = rva in relocations
        checks.append(dict(label='HIGHLOW relocation', rva=f'0x{rva:08x}',
                           expected='HIGHLOW', actual='HIGHLOW' if found else 'missing',
                           matches=found))
    return dict(path=str(path.resolve()), sha256=hashlib.sha256(data).hexdigest(),
                preferred_base=f'0x{base:08x}', checks=checks,
                passed=all(c['matches'] for c in checks),
                boundary='file anchors only; no host admission, native ABI or game acceptance inferred')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, action='append', required=True)
    args = parser.parse_args()
    results = []
    for path in args.exe:
        try:
            results.append(inspect(path))
        except (OSError, ValueError, struct.error) as error:
            results.append(dict(path=str(path), passed=False, error=str(error)))
    print(json.dumps(results, indent=2))
    return 0 if all(r['passed'] for r in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
