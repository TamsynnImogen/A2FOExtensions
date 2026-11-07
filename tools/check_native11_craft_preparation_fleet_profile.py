#!/usr/bin/env python3
"""Pin the Fleet setup wrapper's bounded post-original closure; no execution."""
import argparse,json
from pathlib import Path
from check_native11_engine_inventory import PE32,digest
EXPECTED='3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11'
RANGES={'setup_callback':(0x1fb250,0x1fb2b3),'instance_cache_decoder':(0x1fb18c,0x1fb1ce),
        'craft_class_extension':(0x10c704,0x10c722),'class_cache_decoder':(0x10bc84,0x10bcc6),
        'array_length':(0x6e8c,0x6e94)}
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--image',required=True,type=Path)
 ap.add_argument('--header',required=True,type=Path);ap.add_argument('--receipt',required=True,type=Path);a=ap.parse_args()
 raw=a.image.read_bytes()
 if digest(raw)!=EXPECTED:raise ValueError('unsupported preparation Fleet image')
 p=PE32(raw)
 if p.base!=0x5a800000:raise ValueError('unsupported Fleet preferred base')
 if p.read(0x1fb250,20)!=bytes.fromhex('558bec515356578b5d08894dfc53ff15f0a1a45a'):raise ValueError('saved-original call contract differs')
 if p.read(0x6e8c,8)!=bytes.fromhex('85c074038b40fcc3'):raise ValueError('array length contract differs')
 reloc=p.relocations();regions=[]
 for name,(start,end) in RANGES.items():
  if not 0<end-start<=128 or any(start-3<=r<start or end-3<=r<end for r in reloc):raise ValueError('invalid Fleet region boundary')
  data=p.read(start,end-start);regions.append(dict(name=name,rva=start,size=len(data),bytes_hex=data.hex(),sha256=digest(data),relocations=[r-start for r in reloc if start<=r<end]))
 lines=['// Generated from the exact supported Fleet DLL; bounded base-setup compatibility only.',
 '#pragma once','#include <array>','#include <cstdint>','#include <cstddef>',
 'namespace a2fo::native11_craft_preparation::fleet_profile {',
 'struct Region {std::uint32_t rva;const std::uint8_t* bytes;std::size_t size;const std::uint16_t* relocations;std::size_t relocation_count;};',
 'inline constexpr std::uint32_t preferred_base=0x5a800000;',
 'inline constexpr std::uint32_t callback_rva=0x1fb250,saved_original_rva=0x24a1f0;',
 'inline constexpr std::array<std::uint8_t,32> sha256{{%s}};'%','.join('0x'+EXPECTED[k:k+2] for k in range(0,64,2))]
 for r in regions:
  n=r['name'];lines.append('inline constexpr std::array<std::uint8_t,%d> %s_bytes{{%s}};'%(r['size'],n,','.join('0x%02x'%x for x in bytes.fromhex(r['bytes_hex']))))
  lines.append('inline constexpr std::array<std::uint16_t,%d> %s_fixes{{%s}};'%(len(r['relocations']),n,','.join(map(str,r['relocations']))))
 lines.append('inline constexpr std::array<Region,%d> regions{{'%len(regions))
 for r in regions:
  n=r['name'];lines.append('{0x%x,%s_bytes.data(),%s_bytes.size(),%s_fixes.data(),%s_fixes.size()},'%(r['rva'],n,n,n,n))
 lines+=['}};','}'];a.header.write_text('\n'.join(lines)+'\n')
 a.receipt.write_text(json.dumps(dict(status='Static saved-original wrapper and post-original closure verified; no native execution',image_sha256=EXPECTED,regions=regions,callback_rva=0x1fb250,saved_original_rva=0x24a1f0,header_sha256=digest(a.header.read_bytes()),scope='Base flags/hull algebra only; texture selection, entity tint/FOW and full class behavior unqualified'),indent=2)+'\n')
 print('PASS pinned Fleet base-preparation closure:',len(regions),'regions; no native execution')
if __name__=='__main__':main()
