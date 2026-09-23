#!/usr/bin/env python3
"""Read-only check of animation hook prologues in a PE32 Armada executable."""
import argparse
import re
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    data = args.executable.read_bytes()
    if data[:2] != b'MZ':
        raise SystemExit('Not a PE executable')
    nt = struct.unpack_from('<I', data, 0x3C)[0]
    if data[nt:nt+4] != b'PE\0\0':
        raise SystemExit('Invalid PE signature')
    count = struct.unpack_from('<H', data, nt+6)[0]
    optional = struct.unpack_from('<H', data, nt+20)[0]
    sections = []
    for i in range(count):
        start = nt+24+optional+40*i
        virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', data, start+8)
        sections.append((rva, raw_size, raw))

    def read(rva, size):
        for base, length, raw in sections:
            if base <= rva and rva+size <= base+length:
                return data[raw+rva-base:raw+rva-base+size]
        raise SystemExit(f'Unmapped RVA {rva:08x}')

    source = (Path(__file__).resolve().parents[1] / 'modules/A2FOAnimations/module.cpp').read_text()
    pattern = r'\{(0x[0-9a-f]+),reinterpret_cast<void\*>\((\w+)\),&\w+,\{([^}]+)\}'
    hooks = re.findall(pattern, source)
    if not hooks:
        raise SystemExit('No hook signatures found in module source')
    helpers = re.findall(r'\{(0x[0-9a-f]+),\{([^}]+)\}\}', source)
    hooks.extend((address, 'helper', values) for address, values in helpers)
    for address, name, values in hooks:
        rva = int(address, 16)
        expected = bytes(int(v, 16) for v in values.split(','))
        actual = read(rva, len(expected))
        if actual != expected:
            raise SystemExit(f'{name} {rva:08x}: expected {expected.hex()}, got {actual.hex()}')
        print(f'{name} RVA {rva:08x}: verified')
    print('Static prologues verified; runtime image identity and loaded hooks are checked by the DLL.')


if __name__ == '__main__':
    main()
