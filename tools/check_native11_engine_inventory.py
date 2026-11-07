#!/usr/bin/env python3
"""Inventory reviewed Native11 engine ranges in one pinned PE32 image; never execute it."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


EXPECTED_SHA256 = '4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de'
RANGES = {
    'instance_render': (0x22e780, 0x22e987),
    'instance_animate': (0x22eab0, 0x22eb13),
    'channel_phase': (0x213910, 0x213a7b),
    'start_time': (0x213670, 0x2136ba),
    'force_first': (0x2136c0, 0x2136cb),
    'rounding': (0x244a10, 0x244a30),
    'refresh': (0x224630, 0x224743),
    'first_channel': (0x22f1c0, 0x22f1d3),
    'next_channel': (0x221080, 0x221096),
    'engine_time': (0x22d530, 0x22d537),
    'float_play': (0x2146a0, 0x214720),
    'matrix_play': (0x2161c0, 0x216218),
    'channel_loader_a': (0x23f850, 0x23f9ff),
    'channel_loader_b': (0x23f9ff, 0x23fa5e),
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def u16(data, at):
    if at < 0 or at + 2 > len(data):
        raise ValueError('Truncated PE16 field')
    return struct.unpack_from('<H', data, at)[0]


def u32(data, at):
    if at < 0 or at + 4 > len(data):
        raise ValueError('Truncated PE32 field')
    return struct.unpack_from('<I', data, at)[0]


class PE32:
    def __init__(self, data):
        self.data = data
        if data[:2] != b'MZ':
            raise ValueError('Missing DOS signature')
        pe = u32(data, 0x3c)
        if data[pe:pe + 4] != b'PE\0\0' or u16(data, pe + 4) != 0x14c:
            raise ValueError('Expected i386 PE image')
        count, optional_size = u16(data, pe + 6), u16(data, pe + 20)
        optional = pe + 24
        if optional_size < 0xe0 or optional + optional_size > len(data):
            raise ValueError('Truncated PE32 optional header')
        if u16(data, optional) != 0x10b or u32(data, optional + 92) < 6:
            raise ValueError('Expected PE32 with import and relocation directories')
        self.base = u32(data, optional + 28)
        self.directories = [(u32(data, optional + 96 + i * 8),
                             u32(data, optional + 100 + i * 8)) for i in (1, 5)]
        self.sections = []
        for i in range(count):
            at = optional + optional_size + i * 40
            if at + 40 > len(data):
                raise ValueError('Truncated section table')
            virtual_size, rva, raw_size, offset = struct.unpack_from('<IIII', data, at + 8)
            if raw_size and offset + raw_size > len(data):
                raise ValueError('Section exceeds image file')
            self.sections.append((rva, virtual_size, raw_size, offset))

    def read(self, rva, size):
        if rva < 0 or size < 0:
            raise ValueError('Negative RVA or size')
        for start, virtual_size, raw_size, offset in self.sections:
            if start <= rva and rva + size <= start + min(virtual_size, raw_size):
                at = offset + rva - start
                return self.data[at:at + size]
        raise ValueError(f'RVA 0x{rva:x}+0x{size:x} not file backed')

    def string(self, rva):
        value = bytearray()
        for i in range(256):
            char = self.read(rva + i, 1)[0]
            if not char:
                return value.decode('ascii')
            value.append(char)
        raise ValueError('Unterminated import string')

    def imports(self):
        directory, size = self.directories[0]
        if not directory or size < 20:
            raise ValueError('Missing import directory')
        found = {}
        for at in range(0, size, 20):
            if at + 20 > size:
                raise ValueError('Unterminated import descriptors')
            original, _, _, name, first = struct.unpack('<IIIII', self.read(directory + at, 20))
            if not any((original, name, first)):
                return found
            if not name or not first:
                raise ValueError('Invalid import descriptor')
            dll = self.string(name)
            table = original or first
            for index in range(4096):
                thunk = u32(self.read(table + index * 4, 4), 0)
                if not thunk:
                    break
                if not thunk & 0x80000000:
                    found[first + index * 4] = (dll, self.string(thunk + 2))
            else:
                raise ValueError('Unterminated import thunk table')
        raise ValueError('Unterminated import descriptors')

    def relocations(self):
        directory, size = self.directories[1]
        if not directory or size < 8:
            raise ValueError('Missing relocation directory')
        result = []
        cursor = 0
        while cursor < size:
            if size - cursor < 8:
                raise ValueError('Truncated relocation block')
            page, length = struct.unpack('<II', self.read(directory + cursor, 8))
            if not page and not length:
                if self.read(directory + cursor, size - cursor).strip(b'\0'):
                    raise ValueError('Nonzero relocation padding')
                break
            if length < 8 or length % 2 or cursor + length > size:
                raise ValueError('Invalid relocation block')
            for offset in range(8, length, 2):
                entry = u16(self.read(directory + cursor + offset, 2), 0)
                kind = entry >> 12
                if kind == 3:
                    result.append(page + (entry & 0xfff))
                elif kind != 0:
                    raise ValueError('Unsupported relocation kind')
            cursor += length
        return sorted(result)


def require(pe, rva, hex_bytes, description):
    expected = bytes.fromhex(hex_bytes)
    if pe.read(rva, len(expected)) != expected:
        raise ValueError(f'{description} bytes differ at RVA 0x{rva:x}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--write-clock-header', type=Path)
    args = parser.parse_args()
    data = args.image.read_bytes()
    if digest(data) != EXPECTED_SHA256:
        raise SystemExit('Unreviewed engine SHA-256')
    pe = PE32(data)
    if pe.base != 0x400000:
        raise ValueError('Unexpected image base')

    # Only concrete instruction/data anchors are asserted. Names remain candidate labels.
    for rva, raw, label in (
        (0x22e78a, '8b0d08d57a00', 'engine pointer load'),
        (0x22e794, '898194000000', 'engine +0x94 write'),
        (0x22e81d, '89b200010000', 'current instance +0x100 write'),
        (0x22e84b, 'e8204efeff', 'SetStartTime branch call'),
        (0x22e857, 'e8644efeff', 'ForceFirst branch call'),
        (0x22e872, 'ff5210', 'channel virtual +0x10 call'),
        (0x22e89d, '83c6448b11', 'DB draw prefix A'),
        (0x22e958, '83c6448b01', 'DB draw prefix B'),
        (0x22eabf, 'e8ac4bfeff', 'animation SetStartTime call'),
        (0x22eacb, 'e8f04bfeff', 'animation ForceFirst call'),
        (0x22eaf2, 'ff5010', 'animation channel virtual +0x10 call'),
        (0x213677, 'a1788e7a00', 'start time load'),
        (0x21367c, '890d788e7a00', 'start time write'),
        (0x213696, 'd915748e7a00', 'elapsed time write'),
        (0x2136c0, 'c705748e7a0000000000', 'force first elapsed time clear'),
        (0x21391b, 'b900040000e8eb100300', 'channel rounding call'),
        (0x213925, 'd905748e7a00', 'elapsed time load'),
        (0x213930, 'a108d57a00', 'channel engine pointer load'),
        (0x21399d, 'e882f60500', '_ftol thunk call A'),
        (0x213a46, 'e8d9f50500', '_ftol thunk call B'),
        (0x244a10, '509bd93c24', 'rounding control word read'),
        (0x224710, '8b8690000000508b08', 'final EndScene prefix'),
        (0x224719, 'ff918c000000', 'final EndScene virtual call'),
        (0x224735, 'ff523c', 'Present virtual call'),
        (0x273024, 'ff256c807b00', '_ftol import thunk'),
    ):
        require(pe, rva, raw, label)
    for rva, expected, label in (
        (0x2ae6f0, 0xbf800000, 'minus one float'),
        (0x2bc364, 0x3d4ccccd, '0.05 float'),
    ):
        if u32(pe.read(rva, 4), 0) != expected:
            raise ValueError(f'{label} differs')
    imports = pe.imports()
    if imports.get(0x3b806c) != ('MSVCRT.dll', '_ftol'):
        raise ValueError('_ftol IAT import mismatch')
    slots = ((0x2bc3ac, 0x2146a0), (0x2bc43c, 0x2161c0))
    for slot, target in slots:
        if u32(pe.read(slot, 4), 0) != pe.base + target:
            raise ValueError('Clock channel dispatch ownership mismatch')
    relocs = pe.relocations()
    evidence = {}
    assembly = {}
    for name, (start, end) in RANGES.items():
        raw = pe.read(start, end - start)
        if any(start - 3 <= place < start or end - 3 <= place < end for place in relocs):
            raise ValueError(f'Straddling relocation in {name}')
        fixes = [place - start for place in relocs if start <= place and place + 4 <= end]
        asm = subprocess.check_output([
            'i686-w64-mingw32-objdump', '-d', '-Mintel',
            f'--start-address={hex(pe.base + start)}',
            f'--stop-address={hex(pe.base + end)}', str(args.image)],
            stderr=subprocess.PIPE)
        assembly[name] = asm
        evidence[name] = {
            'rva_start': hex(start), 'rva_end_exclusive': hex(end),
            'size': len(raw), 'bytes_hex': raw.hex(), 'bytes_sha256': digest(raw),
            'relocation_offsets': fixes, 'assembly_sha256': digest(asm),
            'assembly': str(args.output.parent / f'{args.output.stem}-{name}.asm'),
        }
    report = {
        'status': 'static pinned-image inventory; no game code executed',
        'image': str(args.image.resolve()), 'image_sha256': digest(data),
        'preferred_base': hex(pe.base), 'ranges': evidence,
        'import': {'iat_rva': '0x3b806c', 'dll': 'MSVCRT.dll', 'symbol': '_ftol',
                   'thunk_rva': '0x273024'},
        'constants': {'0x2ae6f0': '0xbf800000', '0x2bc364': '0x3d4ccccd'},
        'data_references': {'elapsed_time_va': '0x7a8e74',
                            'start_time_va': '0x7a8e78', 'engine_pointer_va': '0x7ad508'},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for name, asm in assembly.items():
        (args.output.parent / f'{args.output.stem}-{name}.asm').write_bytes(asm)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    if args.write_clock_header:
        names = ('channel_loader_a', 'channel_loader_b',
                 'channel_phase', 'start_time', 'force_first', 'rounding',
                 'first_channel', 'next_channel', 'engine_time', 'float_play', 'matrix_play')
        lines = ['// Generated from the exact pinned image; diagnostics only.', '#pragma once',
                 '#include <array>', '#include <cstdint>', '#include <cstddef>',
                 'namespace a2fo::native11_animation_clock::profile {',
                 'struct Region {std::uint32_t rva; const std::uint8_t* bytes; std::size_t size;',
                 'const std::uint16_t* relocations; std::size_t relocation_count;};',
                 'inline constexpr std::uint32_t preferred_base=0x400000;']
        for name in names:
            item=evidence[name];raw=bytes.fromhex(item['bytes_hex']);fixes=item['relocation_offsets']
            lines.append('inline constexpr std::array<std::uint8_t,%d> %s_bytes{{%s}};' %
                         (len(raw), name, ','.join('0x%02x' % value for value in raw)))
            lines.append('inline constexpr std::array<std::uint16_t,%d> %s_fixes{{%s}};' %
                         (len(fixes), name, ','.join(map(str, fixes))))
        lines.append('inline constexpr std::array<Region,%d> regions{{' % len(names))
        for name in names:
            lines.append('{0x%x,%s_bytes.data(),%s_bytes.size(),%s_fixes.data(),%s_fixes.size()},' %
                         (RANGES[name][0], name, name, name, name))
        lines.extend(['}};', 'inline constexpr std::array<std::array<std::uint32_t,2>,2> slots{{',
                      *['{{0x%x,0x%x}},' % pair for pair in slots], '}};', '}'])
        args.write_clock_header.write_text('\n'.join(lines)+'\n')
    print(f'PASS pinned engine inventory: {len(RANGES)} ranges, _ftol import and data anchors')


if __name__ == '__main__':
    main()
