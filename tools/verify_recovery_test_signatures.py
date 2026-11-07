#!/usr/bin/env python3
"""Read-only check of the observer signatures against the SHA-pinned PE image."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

PIN = '63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36'
DEFAULT_EXE = Path('/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe')
ROOT = Path(__file__).resolve().parents[1]


def verify(exe):
    data = exe.read_bytes()
    if hashlib.sha256(data).hexdigest() != PIN:
        raise ValueError('EXE SHA-256 differs from pinned input')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('invalid PE signatures')
    machine, sections = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    if machine != 0x14c or struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise ValueError('expected PE32/i386')
    if struct.unpack_from('<I', data, optional + 28)[0] != 0x400000:
        raise ValueError('expected image base 0x400000')
    table = optional + optional_size
    header = (ROOT / 'modules/A2FORecoveryTests/signatures.hpp').read_text()
    if f'input_sha256[]="{PIN}"' not in header:
        raise ValueError('header provenance differs')
    checked = []
    for name, rva, length, prefix in (
            ('context', 0x22b710, 17, '8b81c0000000'),
            ('node', 0x239460, 187, '558bec83ec0c')):
        match = re.search(rf'{name}_body\[\]=\{{([^}}]+)\}};', header)
        if not match or f'{name}_rva=0x{rva:x};' not in header:
            raise ValueError(f'{name}: missing pinned declaration')
        expected = bytes(int(token.strip(), 16) for token in match[1].split(','))
        if len(expected) != length or expected[:6].hex() != prefix or expected[-1] != 0xc3:
            raise ValueError(f'{name}: length/displaced-prefix/RET contract differs')
        body = None
        for index in range(sections):
            entry = table + index * 40
            _, va, raw_size, raw_offset = struct.unpack_from('<IIII', data, entry + 8)
            if va <= rva and rva + length <= va + raw_size:
                offset = raw_offset + rva - va
                body = data[offset:offset + length]
                break
        if body != expected:
            raise ValueError(f'{name}: complete file body differs from header')
        checked.append({'routine': name, 'rva': hex(rva), 'bytes': length,
                        'body_sha256': hashlib.sha256(body).hexdigest()})
    return {'status': 'PASS', 'input_sha256': PIN, 'routines': checked}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=DEFAULT_EXE)
    args = parser.parse_args()
    try:
        result = verify(args.exe)
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'signature check failed: {error}\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
