#!/usr/bin/env python3
"""Pin base/Borg operations, constructor table initialization and selector route; no code execution."""
import argparse,json
from pathlib import Path
from check_native11_engine_inventory import PE32,EXPECTED_SHA256,digest
RANGES={'select':(0xbfb70,0xbfbc0),'patched_select':(0x40129e,0x4012e8),
        'set_slot':(0x2313b0,0x2314b7),'build_table':(0xbfc70,0xbfded),
        'count_rows':(0xbfbd0,0xbfc0f),'fill_rows':(0xbfc10,0xbfc66),
        'get_slot':(0x231380,0x2313a3),'mesh_type':(0x1968f0,0x1968f6),
        'craft_selector_prefix':(0xcb0e0,0xcb2a2),
        'craft_selector_tail':(0xcb2a2,0xcb325),
        'constructor_material_table':(0xbf7e6,0xbf957)}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--image',type=Path,required=True);p.add_argument('--header',type=Path,required=True)
 p.add_argument('--receipt',type=Path,required=True);a=p.parse_args()
 raw=a.image.read_bytes()
 if digest(raw)!=EXPECTED_SHA256:raise ValueError('unsupported base/Borg source image')
 pe=PE32(raw);fixes=pe.relocations();regions=[]
 if pe.base!=0x400000:raise ValueError('unexpected image base')
 for name,(start,end) in RANGES.items():
  if not 0<end-start<=512 or any(start-3<=r<start or end-3<=r<end for r in fixes):raise ValueError('invalid range/relocation')
  data=pe.read(start,end-start)
  regions.append(dict(name=name,rva=start,size=len(data),bytes_hex=data.hex(),sha256=digest(data),relocations=[r-start for r in fixes if start<=r<end]))
 lines=['// Generated from the pinned Root executable: base/Borg operations and base Craft selector route.','#pragma once','#include <array>','#include <cstdint>','#include <cstddef>',
 'namespace a2fo::native11_borg_material::profile {',
 'struct Region {std::uint32_t rva;const std::uint8_t* bytes;std::size_t size;const std::uint16_t* relocations;std::size_t relocation_count;};',
 'inline constexpr std::uint32_t preferred_base=0x400000;']
 for r in regions:
  n=r['name'];lines+=['inline constexpr std::array<std::uint8_t,%d> %s_bytes{{%s}};'%(r['size'],n,','.join('0x%02x'%v for v in bytes.fromhex(r['bytes_hex']))),
  'inline constexpr std::array<std::uint16_t,%d> %s_fixes{{%s}};'%(len(r['relocations']),n,','.join(map(str,r['relocations'])))]
 lines+=['inline constexpr std::array<Region,%d> regions{{'%len(regions)]
 for r in regions:
  n=r['name'];lines+=['{0x%x,%s_bytes.data(),%s_bytes.size(),%s_fixes.data(),%s_fixes.size()},'%(r['rva'],n,n,n,n)]
 lines+=['}};','}'];a.header.write_text('\n'.join(lines)+'\n')
 a.receipt.write_text(json.dumps(dict(status='Pinned base/Borg operations, inlined constructor saved-table block and base Craft selector branch; no class-ready, complete caller/module or effective material authority',image_sha256=EXPECTED_SHA256,regions=regions,header_sha256=digest(a.header.read_bytes()),native_code_executed=False,parity_domain='Selector: existing 4..8-slot vectors, no setter allocation path. Constructor table block: fixture slot counts 0/1/4/8 with recorded allocation requests; no native allocator.',preserved_borg_auxiliary_slot=3,selector_route='Craft instance vtable +0x14 -> RVA cb0e0; selector called only when instance +0x80 == class +0x1dc; class from instance +0x98; selector argument from instance +0xa1',selector_call_rva=0xcb2ab,common_render_call_rva=0xcb318),indent=2)+'\n')
 print('PASS bounded base/Borg profile:',len(regions),'regions; base selector branch pinned; class/module readiness remains open')
if __name__=='__main__':main()
