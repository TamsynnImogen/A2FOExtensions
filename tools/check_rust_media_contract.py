#!/usr/bin/env python3
"""Check PE32 media candidates without loading code or using a native SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

class Image:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        pe = self.u32(0x3c)
        assert self.data[:2] == b'MZ' and self.data[pe:pe+4] == b'PE\0\0'
        assert self.u16(pe+4) == 0x14c, 'not i386'
        optional = pe+24
        assert self.u16(optional) == 0x10b, 'not PE32'
        section = optional+self.u16(pe+20)
        self.sections = [struct.unpack_from('<IIII', self.data, section+i*40+8)
                         for i in range(self.u16(pe+6))]
        directory = optional+96
        self.exports = self.read_exports(self.u32(directory), self.u32(directory+4))
        self.imports = self.read_imports(self.u32(directory+8))
    def u16(self, offset): return struct.unpack_from('<H', self.data, offset)[0]
    def u32(self, offset): return struct.unpack_from('<I', self.data, offset)[0]
    def offset(self, rva):
        for virtual_size, virtual, size, raw in self.sections:
            if virtual <= rva < virtual+max(virtual_size, size):
                assert rva-virtual < size, 'unbacked RVA'
                return raw+rva-virtual
        raise ValueError(f'unmapped RVA {rva:x}')
    def string(self, rva):
        start = self.offset(rva)
        end = self.data.index(0, start)
        return self.data[start:end].decode('ascii')
    def read_exports(self, rva, size):
        if not rva: return []
        at = self.offset(rva)
        base, count, names, functions, name_table, ordinals = struct.unpack_from('<IIIIII', self.data, at+16)
        named = {}
        for index in range(names):
            ordinal = self.u16(self.offset(ordinals)+index*2)
            named[ordinal] = self.string(self.u32(self.offset(name_table)+index*4))
        result = []
        for index in range(count):
            target = self.u32(self.offset(functions)+index*4)
            if target:
                result.append((base+index, named.get(index), self.string(target) if rva <= target < rva+size else None))
        return result
    def read_imports(self, rva):
        result = []
        if not rva: return result
        at = self.offset(rva)
        while any(struct.unpack_from('<IIIII', self.data, at)):
            result.append(self.string(self.u32(at+12)).lower())
            at += 20
        return sorted(set(result))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, default=Path('build'))
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    receipt = {'status': 'checking', 'images': {}}
    for name in ['A2FOExtensions.dll', 'modules/A2FOVideo.dll', 'A2FOMedia.dll']:
        image = Image(args.candidate/name)
        forbidden = r'(?:ffmpeg|avcodec|avformat|avutil|swscale|swresample)'
        assert not any(re.search(forbidden, name, re.I) for name in image.imports)
        symbols = subprocess.check_output(['i686-w64-mingw32-nm', '-g', str(image.path)], text=True)
        assert not re.search(r'\b_?(?:avcodec|avformat|avutil|av|sws|swr)_\w+', symbols), name
        if name != 'A2FOMedia.dll':
            baseline = Image(args.baseline/name)
            assert image.exports == baseline.exports, f'{name}: export names/ordinals/forwarders changed'
            assert set(image.imports) <= set(baseline.imports), f'{name}: added direct dependency'
        else:
            assert image.exports == [(1, 'A2FOMedia_GetApiV1', None)]
        receipt['images'][name] = {'sha256': hashlib.sha256(image.data).hexdigest(),
                                  'exports': image.exports, 'imports': image.imports}
    receipt['status'] = 'passed-pe32-contract-and-no-ffmpeg-symbols'
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2)+'\n')
    for name, result in receipt['images'].items():
        print(f'{name}: {len(result["exports"])} exports, {len(result["imports"])} dependencies, no FFmpeg symbols')
if __name__ == '__main__': main()
