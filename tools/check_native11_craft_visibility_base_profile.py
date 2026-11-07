#!/usr/bin/env python3
"""Pin base node-flag policy only, excluding material and native draw calls."""
import argparse,json,struct
from pathlib import Path
from check_native11_engine_inventory import PE32,EXPECTED_SHA256,digest
RANGES={'craft_roles':(0xcb0e0,0xcb2a2),'borg_role':(0xcb2b0,0xcb312),
        'damage_walk':(0xcb330,0xcb390),'common_flags':(0xd59b0,0xd5a0d),
        'damage_one':(0x2ae8f0,0x2ae8f8),'damage_step':(0x2aea10,0x2aea14)}
SLOTS=((0x2b3e40,0xcb0e0),)
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--image',type=Path,required=True)
 ap.add_argument('--header',type=Path,required=True);ap.add_argument('--receipt',type=Path,required=True);a=ap.parse_args()
 raw=a.image.read_bytes()
 if digest(raw)!=EXPECTED_SHA256:raise ValueError('unsupported base visibility image')
 pe=PE32(raw)
 if pe.base!=0x400000 or pe.read(0x2ae8f0,8)!=struct.pack('<d',1) or pe.read(0x2aea10,4)!=struct.pack('<f',.2):raise ValueError('visibility constants/base differ')
 # Excluded material callback and final render call remain explicitly outside
 # this bounded proof; their existence is not normalized into an authority claim.
 if pe.read(0xcb2ab,5)!=bytes.fromhex('e8c048ffff') or pe.read(0xcb318,5)!=bytes.fromhex('e893a60000'):raise ValueError('visibility exclusion boundaries differ')
 regions=[];relocs=pe.relocations()
 for name,(start,end) in RANGES.items():
  if not 0<end-start<=512 or any(start-3<=r<start or end-3<=r<end for r in relocs):raise ValueError('invalid visibility range/relocation')
  data=pe.read(start,end-start);regions.append(dict(name=name,rva=start,size=len(data),bytes_hex=data.hex(),sha256=digest(data),relocations=[r-start for r in relocs if start<=r<end]))
 for slot,target in SLOTS:
  if pe.read(slot,4)!=struct.pack('<I',pe.base+target):raise ValueError('base visibility slot differs')
 lines=['// Generated from the pinned Root executable: base flag policy only.','#pragma once','#include <array>','#include <cstdint>','#include <cstddef>',
 'namespace a2fo::native11_craft_visibility_base::profile {',
 'struct Region {std::uint32_t rva;const std::uint8_t* bytes;std::size_t size;const std::uint16_t* relocations;std::size_t relocation_count;bool code;};',
 'inline constexpr std::uint32_t preferred_base=0x400000;']
 for r in regions:
  n=r['name'];lines+=['inline constexpr std::array<std::uint8_t,%d> %s_bytes{{%s}};'%(r['size'],n,','.join('0x%02x'%v for v in bytes.fromhex(r['bytes_hex']))),
  'inline constexpr std::array<std::uint16_t,%d> %s_fixes{{%s}};'%(len(r['relocations']),n,','.join(map(str,r['relocations'])))]
 lines+=['inline constexpr std::array<Region,%d> regions{{'%len(regions)]
 for r in regions:
  n=r['name'];lines+=['{0x%x,%s_bytes.data(),%s_bytes.size(),%s_fixes.data(),%s_fixes.size(),%s},'%(r['rva'],n,n,n,n,'true' if r['rva']<0x200000 else 'false')]
 lines+=['}};','inline constexpr std::array<std::array<std::uint32_t,2>,1> slots{{{{0x2b3e40,0xcb0e0}}}};','}']
 a.header.write_text('\n'.join(lines)+'\n')
 a.receipt.write_text(json.dumps(dict(status='Pinned base node flags only; material/FOW/final draw excluded',image_sha256=EXPECTED_SHA256,regions=regions,slots=SLOTS,excluded_material_call_rva=0xcb2ab,excluded_final_draw_call_rva=0xcb318,header_sha256=digest(a.header.read_bytes()),native_code_executed=False),indent=2)+'\n')
 print('PASS base visibility profile:',len(regions),'regions and one slot; no material/full visibility claim')
if __name__=='__main__':main()
