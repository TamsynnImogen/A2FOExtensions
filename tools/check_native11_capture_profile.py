#!/usr/bin/env python3
"""Pin static native11 capture boundaries without loading/executing the game."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

EXPECTED = '4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de'
REGIONS = {
    'entity-render': (0xCFDA0, 0xCFDCA),
    'entity-transform': (0xCFD50, 0xCFD57),
    'object-visible-render': (0xD54E0, 0xD550F),
    'cpu-project-point': (0x218890, 0x218960),
    'cpu-projection-builder': (0x2310C0, 0x23118A),
    'camera-combined-rebuild': (0x2198E0, 0x219C00),
    'instance-selected-database-render': (0x22E780, 0x22E970),
    'instance-set-database': (0x22EED0, 0x22EF25),
    'plain-database-name-constructor': (0x220540, 0x220611),
    'database-name-string-copy': (0x1908B0, 0x1908F9),
    'stream-database-construction': (0x23FA60, 0x23FAE0),
    'load-database-name-forwarding': (0x22CE45, 0x22CEAB),
    'craft-instance-frustum-scale': (0xCB6B0, 0xCB6D6),
    'apply-scale-effects': (0x199760, 0x1997FB),
    'instance-scale-copy': (0x22E7E3, 0x22E816),
    'plain-database-basis-scale': (0x2209DC, 0x220A9F),
}
SCALE_REGIONS = ('craft-instance-frustum-scale', 'apply-scale-effects',
                 'instance-scale-copy', 'plain-database-basis-scale')
CHECKS = {
    'transform-accessor': (0xCFD50, '8b410483c044c3'),
    'post-frustum-tap': (0xCFDBF, '8b11ff5214'),
    'frustum-and-onscreen': (0xCFDB0, 'ff500c84c0740d8b4e04c646250157'),
    'object-visibility-gate': (0xD54E6, '8a462784c0751d8b06ff500884c07414'),
    'main-view-call': (0x19886E, 'e89df8ffff'),
    'viewport-endpoint-constants': (0x2BC468, '000080bf000080bf'),
    'selected-database-render': (0x22E897, '8b8e8000000083c6448b1156ddd8ff12'),
    'plain-database-vtable': (0x220583, 'c7066cc56b00'),
    'copied-database-name': (0x220595, 'e81603f7ff894638'),
    'database-constructor-name-argument': (0x23FAB5, '8b4d0c518bc8e8800afeff'),
}
def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--write-scale-header', type=Path)
    parser.add_argument('--check-scale-header', type=Path)
    args = parser.parse_args()
    data = args.image.read_bytes()
    if digest(data) != EXPECTED:
        raise SystemExit('Refusing unreviewed engine SHA-256')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise SystemExit('Invalid PE header')
    count = struct.unpack_from('<H', data, pe+6)[0]
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe+24
    if struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise SystemExit('Expected PE32')
    base = struct.unpack_from('<I', data, optional+28)[0]
    sections=[]
    for i in range(count):
        at=optional+optional_size+i*40
        name=data[at:at+8].rstrip(b'\0').decode('ascii')
        virtual_size,rva,raw_size,offset=struct.unpack_from('<IIII',data,at+8)
        sections.append((name,rva,raw_size,offset,virtual_size))
    def read(rva,size):
        for _,start,raw,offset,_ in sections:
            if start<=rva and rva+size<=start+raw:
                return data[offset+rva-start:offset+rva-start+size]
        raise ValueError('RVA is not fully file-backed')
    reloc_rva,reloc_size=struct.unpack_from('<II',data,optional+96+5*8)
    relocations=[]
    cursor=0
    while cursor<reloc_size:
        page,length=struct.unpack('<II',read(reloc_rva+cursor,8))
        if not page and not length:break
        if length<8 or length%2 or cursor+length>reloc_size:
            raise ValueError('Invalid exact-target relocation block')
        for off in range(8,length,2):
            entry=struct.unpack('<H',read(reloc_rva+cursor+off,2))[0]
            if entry>>12==3:relocations.append(page+(entry&0xfff))
            elif entry>>12!=0:raise ValueError('Unsupported exact-target relocation kind')
        cursor+=length
    header=['// Generated from the pinned target by check_native11_capture_profile.py.',
            '// HIGHLOW fields are normalized to the preferred image base before comparison.',
            '#pragma once','#include <array>','#include <cstddef>','#include <cstdint>',
            'namespace a2fo::native11_capture::profile {',
            'struct Region {std::uint32_t rva;const std::uint8_t* bytes;std::size_t size;',
            '    const std::uint16_t* relocations;std::size_t relocation_count;};',
            'inline constexpr std::uint32_t preferred_base=0x%08x;'%base]
    names=[]
    for name in SCALE_REGIONS:
        start,end=REGIONS[name];code=read(start,end-start);label=name.replace('-','_')
        relocs=[at-start for at in relocations if start<=at and at+4<=end]
        if any(start-3<=at<start or end-3<=at<end for at in relocations):
            raise ValueError('Relocation straddles scale guard region')
        header.append('inline constexpr std::array<std::uint8_t,%d> %s_bytes{{'%(len(code),label))
        for off in range(0,len(code),16):header.append('    '+','.join('0x%02x'%v for v in code[off:off+16])+',')
        header.append('}};')
        header.append('inline constexpr std::array<std::uint16_t,%d> %s_relocations{{%s}};'%(len(relocs),label,','.join(str(v) for v in relocs)))
        names.append((label,start))
    header.append('inline constexpr std::array<Region,%d> regions{{'%len(names))
    for label,start in names:header.append('    {0x%08x,%s_bytes.data(),%s_bytes.size(),%s_relocations.data(),%s_relocations.size()},'%(start,label,label,label,label))
    header.extend(['}};','}'])
    header_text='\n'.join(header)+'\n'
    if args.check_scale_header and args.check_scale_header.read_text()!=header_text:
        raise SystemExit('Scale guard header differs from exact-target generation')
    if args.write_scale_header:args.write_scale_header.write_text(header_text)
    checks={}
    for name,(rva,expected) in CHECKS.items():
        actual=read(rva,len(bytes.fromhex(expected))).hex()
        if actual!=expected:
            raise SystemExit('Signature mismatch: '+name)
        checks[name]={'rva':hex(rva),'bytes':actual}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    regions={}
    for name,(start,end) in REGIONS.items():
        out=args.output.parent/('capture-'+name+'.asm')
        assembly=subprocess.check_output(['i686-w64-mingw32-objdump','-d','-Mintel',
            '--start-address='+hex(base+start),'--stop-address='+hex(base+end),str(args.image)])
        out.write_bytes(assembly)
        regions[name]={'rva_start':hex(start),'rva_end':hex(end),
                       'code_sha256':digest(read(start,end-start)),
                       'assembly':str(out),'assembly_sha256':digest(assembly)}
    result={'status':'static verified; no game entrypoint executed',
        'image':str(args.image.resolve()),'image_sha256':EXPECTED,
        'checks':checks,'regions':regions,
        'camera_conversion':'Transpose combined row-vector matrix at camera+0x160; clipX=2*(w-1)/w*U-W; clipY=W-2*(h-1)/h*V; clipZ=Z; clipW=W. Native viewport reproduces ProjectPoint pixel coordinates.',
        'placement':'Entity::GetTransform reads instance at entity+4 and returns Matrix34 at instance+0x44; telemetry copies those floats without replaying native methods.',
        'capture_boundary':'Entity::Render successful instance frustum test and on-screen byte write precede tap. The existing main-view wrapper supplies view ownership. GameObject::Render has an earlier per-object visibility gate.',
        'runtime_scale':'Verified CraftInstance vtable slot3 applies scale before frustum. Plain database scales placement basis rows, leaves translation unchanged, then composes node transform. Captured native column-vector placement scales columns. Baked per-node/ScaleSOD mapping remains unverified.',
        'limits':['No live game acceptance','Not every class uses Entity::Render',
                  'Placement is not full effective scale, animation, material or model identity',
                  'Model name is copied from the selected plain database, not guessed from the ODF',
                  'File-to-loaded geometry scaling/variants still need parity','No graphics takeover']}
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'PASS exact native11 capture profile, {len(checks)} signatures, {len(regions)} disassembly regions')

if __name__=='__main__':
    main()
