#!/usr/bin/env python3
"""File-only FO menu binding audit; no game/DLL execution or binary modification."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

def inspect(path, source):
    data=path.read_bytes()
    if data[:2]!=b'MZ':raise ValueError('not MZ')
    pe=struct.unpack_from('<I',data,60)[0]
    if data[pe:pe+4]!=b'PE\0\0':raise ValueError('not PE')
    machine,count=struct.unpack_from('<HH',data,pe+4)
    opt_size=struct.unpack_from('<H',data,pe+20)[0];opt=pe+24
    if machine!=0x14c or struct.unpack_from('<H',data,opt)[0]!=0x10b:raise ValueError('not PE32 i386')
    base=struct.unpack_from('<I',data,opt+28)[0]
    sections=[]
    for n in range(count):
        at=opt+opt_size+n*40;_,rva,raw_size,raw=struct.unpack_from('<IIII',data,at+8)
        sections.append((rva,raw_size,raw))
    def read(rva,size):
        for start,length,raw in sections:
            at=rva-start
            if 0<=at<=length and size<=length-at and raw+at+size<=len(data):return data[raw+at:raw+at+size]
        raise ValueError(f'unbacked RVA {rva:#x}')
    checks=[]
    for rva,encoded,length in re.findall(r'\{(show_rva|0x[0-9a-f]+),"([^"]+)",(\d+)\}',source):
        site=0x1cc330 if rva=='show_rva' else int(rva,16)
        expected=bytes.fromhex(encoded.replace('\\x',''))
        if len(expected)!=int(length):raise ValueError('source signature size mismatch')
        actual=read(site,len(expected))
        checks.append(dict(rva=f'0x{site:08x}',expected=expected.hex(),actual=actual.hex(),matches=actual==expected))
    if len(checks)!=30:raise ValueError('expected all thirty main-menu and shared-skin signatures')
    vmt=0x1cb7a8
    name_ptr,instance_size=struct.unpack('<II',read(vmt-44,8))
    class_ok=base==0x5a800000 and instance_size==0x3b4 and name_ptr>=base and read(name_ptr-base,10)==b'\x09TMainForm'
    crc=zlib.crc32(data)
    sha=hashlib.sha256(data).hexdigest()
    profile=len(data)==3043840 and (crc in (0x07bd1e4f,0x3ed28fbe,0x46a0dd96) or
        (crc==0x7cd6661e and sha=='3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11'))
    return dict(path=str(path.resolve()),sha256=hashlib.sha256(data).hexdigest(),crc32=f'0x{crc:08x}',
                known_complete_file_profile=profile,main_form_class_and_size=class_ok,checks=checks,
                passed=profile and class_ok and all(c['matches'] for c in checks),
                boundary='file evidence only; no runtime UI or game acceptance')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fo',type=Path,action='append',required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];source=(root/'modules/A2FORefitMenus/bindings.hpp').read_text()+(root/'modules/A2FORefitMenus/skin_bindings.hpp').read_text();results=[]
    for path in a.fo:
        try:results.append(inspect(path,source))
        except (OSError,ValueError,struct.error) as e:results.append(dict(path=str(path),passed=False,error=str(e)))
    print(json.dumps(results,indent=2));return int(not all(r['passed'] for r in results))
if __name__=='__main__':raise SystemExit(main())
