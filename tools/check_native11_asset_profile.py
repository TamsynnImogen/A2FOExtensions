#!/usr/bin/env python3
"""Generate/check the exact opened-model observation profile; never runs code."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

EXPECTED='4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de'
REGIONS={
    'model_read_call':(0x22ce45,0x22ce57),
    'read_database_entry':(0x23fa60,0x23fa9c),
    'read_database_return':(0x23fc1a,0x23fc3e),
    'buffered_file_open':(0x240150,0x2401cf),
    'buffered_file_read':(0x240240,0x240297),
}
SLOTS=((0x2bc87c,0x240240),(0x2bc884,0x240260),
       (0x2bc888,0x2401d0),(0x2bc88c,0x240210))
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--write-header',type=Path)
    p.add_argument('--check-header',type=Path)
    args=p.parse_args();data=args.image.read_bytes()
    if sha(data)!=EXPECTED:raise SystemExit('Unreviewed engine SHA-256')
    pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[pe:pe+4]!=b'PE\0\0':raise SystemExit('Invalid PE')
    count=struct.unpack_from('<H',data,pe+6)[0];opt_size=struct.unpack_from('<H',data,pe+20)[0];opt=pe+24
    if struct.unpack_from('<H',data,opt)[0]!=0x10b:raise SystemExit('Expected PE32')
    base=struct.unpack_from('<I',data,opt+28)[0];sections=[]
    for i in range(count):
        at=opt+opt_size+i*40
        _,rva,size,offset=struct.unpack_from('<IIII',data,at+8);sections.append((rva,size,offset))
    def read(rva,size):
        for start,length,offset in sections:
            if start<=rva and rva+size<=start+length:return data[offset+rva-start:offset+rva-start+size]
        raise ValueError('RVA not entirely file-backed')
    rr,rs=struct.unpack_from('<II',data,opt+96+5*8);relocs=[];cursor=0
    while cursor<rs:
        page,length=struct.unpack('<II',read(rr+cursor,8))
        if not page and not length:break
        if length<8 or length%2 or cursor+length>rs:raise ValueError('Invalid relocation block')
        for offset in range(8,length,2):
            entry=struct.unpack('<H',read(rr+cursor+offset,2))[0]
            if entry>>12==3:relocs.append(page+(entry&0xfff))
            elif entry>>12:raise ValueError('Unsupported relocation kind')
        cursor+=length
    if read(0x22ce4d,5)!=bytes.fromhex('e80e2c0100'):raise SystemExit('Model call mismatch')
    for slot,target in SLOTS:
        if struct.unpack('<I',read(slot,4))[0]!=base+target:raise SystemExit('Stream vtable mismatch')
    out=['// Generated from the pinned target by check_native11_asset_profile.py.',
         '#pragma once','#include <array>','#include <cstdint>','#include <cstddef>',
         'namespace a2fo::native11_assets::profile {',
         'struct Region {std::uint32_t rva;const std::uint8_t* bytes;std::size_t size;',
         '    const std::uint16_t* relocations;std::size_t relocation_count;};',
         'inline constexpr std::uint32_t preferred_base=0x%08x;'%base]
    args.output.parent.mkdir(parents=True,exist_ok=True);evidence={}
    for name,(start,end) in REGIONS.items():
        raw=read(start,end-start);fixups=[x-start for x in relocs if start<=x and x+4<=end]
        if any(start-3<=x<start or end-3<=x<end for x in relocs):raise ValueError('Straddling relocation')
        out.append('inline constexpr std::array<std::uint8_t,%d> %s_bytes{{'%(len(raw),name))
        for at in range(0,len(raw),16):out.append('    '+','.join('0x%02x'%x for x in raw[at:at+16])+',')
        out.append('}};')
        out.append('inline constexpr std::array<std::uint16_t,%d> %s_relocations{{%s}};'%(len(fixups),name,','.join(map(str,fixups))))
        asm=subprocess.check_output(['i686-w64-mingw32-objdump','-d','-Mintel','--start-address='+hex(base+start),'--stop-address='+hex(base+end),str(args.image)])
        path=args.output.parent/('asset-'+name+'.asm');path.write_bytes(asm)
        evidence[name]={'rva_start':hex(start),'rva_end':hex(end),'bytes_sha256':sha(raw),'relocations':fixups,'assembly':str(path),'assembly_sha256':sha(asm)}
    out.append('inline constexpr std::array<Region,%d> regions{{'%len(REGIONS))
    for name,(start,_) in REGIONS.items():out.append('    {0x%08x,%s_bytes.data(),%s_bytes.size(),%s_relocations.data(),%s_relocations.size()},'%(start,name,name,name,name))
    out.append('}};');out.append('inline constexpr std::array<std::array<std::uint32_t,2>,%d> slots{{'%len(SLOTS))
    for slot,target in SLOTS:out.append('    {{0x%08x,0x%08x}},'%(slot,target))
    out.extend(['}};','}']);header='\n'.join(out)+'\n'
    if args.check_header and args.check_header.read_text()!=header:raise SystemExit('Generated asset header mismatch')
    if args.write_header:args.write_header.write_text(header)
    args.output.write_text(json.dumps({'status':'static profile verified; no game code executed','image':str(args.image.resolve()),'sha256':EXPECTED,'regions':evidence,'slots':SLOTS,'call':'ReadDatabase thiscall, ECX=buffered stream, two stack arguments; ret8. Arg1 scalar bits, arg2 copied database name.','data':'stream+14 begin, +18 actual-read end, +1c cursor. Requested length not retained; no complete-file or baked-scale claim.'},indent=2)+'\n')
    print('PASS exact native model asset profile:',len(REGIONS),'loaded regions,',len(SLOTS),'stream slots')
if __name__=='__main__':main()
