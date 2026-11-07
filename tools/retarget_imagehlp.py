#!/usr/bin/env python3
"""Offline, hash-gated IMAGEHLP -> system DBGHELP import migration for two images.

Only private build/ outputs are written. This preserves the imported function,
thunk addresses, code, resources and image geometry; no game code is executed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

from armada_compat_profile import parse_image, make_profile
from check_rust_media_contract import Image

ROOT = Path(__file__).resolve().parents[1]
KNOWN = {
    "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de": "roots-oct23",
    "c8846226048931134e07d48702309b2fec1ef0f67f5b4d82bec0eb5249257d8c": "fo4-parent",
}

def sha(data):
    return hashlib.sha256(data).hexdigest()

class BytesImage(Image):
    def __init__(self, data):
        self.data = data
        pe = self.u32(0x3c)
        optional = pe+24
        self.optional = optional
        at = optional+self.u16(pe+20)
        self.sections = [struct.unpack_from('<IIII', data, at+i*40+8) for i in range(self.u16(pe+6))]

def normal_imports(data):
    im = BytesImage(data)
    directory = im.optional+96
    rva = im.u32(directory+8)
    size = im.u32(directory+12)
    if not rva or size < 20:
        raise ValueError('normal import directory missing')
    at = im.offset(rva)
    result = []
    for index in range(size//20):
        oft, timestamp, chain, name, first = struct.unpack_from('<IIIII', data, at+index*20)
        if not any((oft,timestamp,chain,name,first)):
            return result
        if timestamp or not oft:
            raise ValueError('bound imports or missing original thunk table not supported')
        entries = []
        pos = im.offset(oft)
        for entry in range(len(data)//4):
            value = im.u32(pos+entry*4)
            if value == 0:
                break
            if value & 0x80000000:
                entries.append({'ordinal':value&0xffff,'iat_rva':first+entry*4})
            else:
                entries.append({'name':im.string(value+2),'iat_rva':first+entry*4})
        else:
            raise ValueError('unterminated thunk table')
        result.append({'dll':im.string(name), 'name_offset':im.offset(name), 'entries':entries})
    raise ValueError('unterminated import directory')

def checksum(data, field):
    total = 0
    for i in range(0, len(data), 2):
        word = 0 if field <= i < field+4 else int.from_bytes(data[i:i+2], 'little')
        total += word
        total = (total & 0xffff)+(total >> 16)
    total = (total & 0xffff)+(total >> 16)
    return (total & 0xffff)+len(data)

def rewrite(data):
    """Internal pure byte transform; production CLI additionally requires KNOWN hash."""
    before_info = parse_image(data)
    before = normal_imports(data)
    selected = [x for x in before if x['dll'].lower() == 'imagehlp.dll']
    if len(selected) != 1 or [x.get('name') for x in selected[0]['entries']] != ['UnDecorateSymbolName']:
        raise ValueError('expected exactly one ImageHlp import of UnDecorateSymbolName')
    if any(x['dll'].lower() == 'dbghelp.dll' for x in before):
        raise ValueError('existing DbgHelp descriptor requires separate audit')
    item = selected[0]
    old = item['dll'].encode('ascii')+b'\0'
    new = b'DBGHELP.dll\0'
    if len(new) > len(old):
        raise ValueError('replacement cannot fit the existing name slot')
    at = item['name_offset']
    if data[at:at+len(old)] != old:
        raise ValueError('import-name bytes differ')
    output = bytearray(data)
    output[at:at+len(old)] = new+b'\0'*(len(old)-len(new))
    field = BytesImage(data).optional+64
    struct.pack_into('<I', output, field, checksum(output, field))
    output = bytes(output)
    after = normal_imports(output)
    expected = [dict(x, dll='DBGHELP.dll') if x is item else x for x in before]
    if after != expected:
        raise ValueError('unexpected import/thunk change')
    allowed = set(range(at,at+len(old))) | set(range(field,field+4))
    differences = [i for i,(a,b) in enumerate(zip(data,output)) if a != b]
    if len(data) != len(output) or not set(differences) <= allowed:
        raise ValueError('unexpected bytes changed')
    after_info = parse_image(output)
    for a,b in zip(before_info['sections'],after_info['sections']):
        if a['name'] != '.idata' and a != b:
            raise ValueError('a non-import section changed')
    return output, {'before_sha256':sha(data),'after_sha256':sha(output),
                    'function':'UnDecorateSymbolName','unchanged_iat_rva':item['entries'][0]['iat_rva'],
                    'old_dll':item['dll'],'new_dll':'DBGHELP.dll','changed_file_offsets':differences,
                    'checksum':BytesImage(output).u32(field),
                    'section_geometry_unchanged':True,'executable_sections_unchanged':True}

def canonical(path):
    path = Path(path).absolute()
    for part in (path,*path.parents):
        if part.is_symlink():
            raise ValueError(f'symlink input/output path: {part}')
    return path.resolve()

def patch(source, output):
    source, output = canonical(source), canonical(output)
    if not output.is_relative_to(ROOT/'build') or output == ROOT/'build' or output.exists():
        raise ValueError('output must be a fresh file under repository build/')
    if not source.is_file() or source == output:
        raise ValueError('source must be an existing separate image')
    data = source.read_bytes()
    digest = sha(data)
    if digest not in KNOWN:
        raise ValueError(f'unknown engine image {digest}; no patch applied')
    changed, record = rewrite(data)
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('xb') as stream:
        stream.write(changed)
    if sha(source.read_bytes()) != digest:
        output.unlink()
        raise ValueError('source changed during patch; output is not accepted')
    record.update(status='patched_private_image_runtime_pending', source=str(source), output=str(output), profile=KNOWN[digest],
                  platform_dependency='system DBGHELP.dll; not a Rust undecorator or a guarantee of old ImageHlp output parity')
    if KNOWN[digest] == 'roots-oct23':
        profile = output.with_name(output.name+'.profile.txt')
        profile.write_text(make_profile(parse_image(changed)))
        record['profile_proposal'] = str(profile)
    output.with_name(output.name+'.receipt.json').write_text(json.dumps(record,indent=2)+'\n')
    return record

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(patch(args.source,args.output),indent=2))

if __name__ == '__main__':
    try:
        main()
    except (ValueError,OSError,struct.error,AssertionError,IndexError) as error:
        raise SystemExit(f'retarget_imagehlp: {error}')
