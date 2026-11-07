#!/usr/bin/env python3
"""Pin the four engine methods used only by the isolated diagnostic scene."""
import hashlib,json,os,struct
from pathlib import Path
from check_refit_fo_profile import PE,PROFILES
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
paths=[Path('/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe'),Path('/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0/ArmadaL.exe')]
images=[]
for path in paths:
 data=path.read_bytes();sha=hashlib.sha256(data).hexdigest();assert sha in PROFILES;images.append((PE(data),sha))
p=images[0][0];rva,size=struct.unpack('<II',p.raw(p.opt+96+5*8,8));relocs=[];at=0
while at<size:
 page,n=struct.unpack('<II',p.read(rva+at,8));assert n>=8
 for offset in range(8,n,2):
  v=struct.unpack('<H',p.read(rva+at+offset,2))[0]
  if v>>12==3:relocs.append(page+(v&0xfff))
 at+=n
regions=[('build',0x51990,0x56),('fog',0x935d0,0x30),('update',0x93600,0x1e),('center',0x55190,0x33)]
lines=['// Generated from both exact reviewed FO files; diagnostic calls only.','#pragma once','#include <array>','#include <cstdint>','#include <cstddef>','namespace a2fo::refit_scene_test::profile {','struct Region {std::uint32_t rva;const std::uint8_t* bytes;std::size_t size;const std::uint16_t* relocations;std::size_t relocation_count;};']
records=[]
for key,rva,n in regions:
 b=p.read(rva,n);assert all(q.read(rva,n)==b for q,_ in images)
 fixes=[v-rva for v in relocs if rva<=v<rva+n];assert all(v+4<=n for v in fixes)
 lines += [f'inline constexpr std::array<std::uint8_t,{n}> {key}_bytes{{{{'+','.join(f'0x{v:02x}' for v in b)+'}};',f'inline constexpr std::array<std::uint16_t,{len(fixes)}> {key}_fixes{{{{'+','.join(map(str,fixes))+'}};']
 records.append({'name':key,'rva':rva,'bytes':b.hex(),'relocations':fixes})
lines+=['inline constexpr Region regions[]={']+[f'{{0x{rva:x},{key}_bytes.data(),{key}_bytes.size(),{key}_fixes.data(),{key}_fixes.size()}},' for key,rva,n in regions]+['};','}']
Path('core/refit_scene_test_profile.hpp').write_text('\n'.join(lines)+'\n')
out=Path('build/refit-scene-test-profile');out.mkdir(exist_ok=True);(out/'receipt.json').write_text(json.dumps({'images':[{'path':str(path),'sha256':sha} for path,(_,sha) in zip(paths,images)],'regions':records},indent=2)+'\n')
