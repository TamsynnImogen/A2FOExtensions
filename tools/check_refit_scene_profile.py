#!/usr/bin/env python3
"""Qualify the narrow live camera/light copy fields in both pinned FO images.

Reads original executables only. Writes the generated C++ profile and evidence
under this checkout; it never executes a game or patches a binary.
"""
import sys,struct,json,hashlib,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
sys.path.insert(0,str(ROOT/'tools'))
from check_refit_fo_profile import PE,PROFILES
paths=[Path('/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe'),Path('/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0/ArmadaL.exe')]
# No native method is called: these exact code spans qualify copied fields.
regions=[('ambient_copy',0x19819e,0x27),('current_team_copy',0xd53f9,0x19),('camera_position_getter',0x2194d0,7),('light_record_constructor',0x22d130,0x48),('light_list',0x22d293,0x3e),('point_constructor',0x22f240,0x6a),('workspace_context',0x13e470,0x37),('mesh_render_dispatch',0x2327cf,0x5b),('mesh_fast_body',0x231f10,0xb2),('mesh_legacy_body_a',0x231fd0,512),('mesh_legacy_body_b',0x2321d0,512),('mesh_legacy_body_c',0x2323d0,499),('mesh_workspace_begin',0x247b80,0x78),('mesh_workspace_submit',0x248af0,0x140),('mesh_material_selection',0x232715,0x3f),('mesh_workspace_vtable',0x2bcd8c,28)]
# Native self-illuminating materials submit a lit base pass followed by an
# alpha-blended emissive pass. Only their base draw may be replaced. Qualify the
# class and complete relevant bodies in both original images before using it.
regions += [('mesh_material_self_vtable',0x2bc854,28),
            ('mesh_material_self_begin',0x22dc40,8),
            ('mesh_material_self_end',0x22dc30,1),
            ('mesh_material_self_pass',0x23a0b0,0x73),
            ('mesh_material_base_pass',0x2446a0,0x5d),
            ('mesh_material_prepare',0x244760,0x2e),
            ('mesh_material_budget',0x245cf0,0x94),
            ('mesh_material_lighting_vtable',0x2bc9a4,8),
            ('mesh_material_named_ctor',0x220f70,0x2f),
            ('mesh_material_name_copy',0x1908b0,0x47),
            ('mesh_material_lighting_ctor',0x230600,0xd2),
            ('mesh_material_lighting_reader',0x23f4e0,0x1ae),
            ('mesh_material_lighting_copy',0x2481d0,0xc4)]
images=[]
for p in paths:
 b=p.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha in PROFILES
 images.append((PE(b),sha))
pe=images[0][0]
reloc_rva,reloc_size=struct.unpack('<II',pe.raw(pe.opt+96+5*8,8));relocs=[];pos=0
while pos<reloc_size:
 page,size=struct.unpack('<II',pe.read(reloc_rva+pos,8))
 assert size>=8 and size%2==0
 for at in range(8,size,2):
  item=struct.unpack('<H',pe.read(reloc_rva+pos+at,2))[0]
  if item>>12==3:relocs.append(page+(item&0xfff))
 pos+=size
out=['// Qualified against Root4300 and FO4c884, original files unchanged.', '#pragma once','#include <array>','#include <cstdint>','#include <cstddef>','namespace a2fo::refit_scene::profile {','struct Region {std::uint32_t rva; const std::uint8_t* bytes;std::size_t size;const std::uint16_t* relocations;std::size_t relocation_count;};']
records=[]
for key,rva,size in regions:
 b=pe.read(rva,size);assert all(p.read(rva,size)==b for p,s in images)
 fixes=[r-rva for r in relocs if rva<=r<rva+size];assert all(f+4<=size for f in fixes)
 out += [f'inline constexpr std::array<std::uint8_t,{size}> {key}_bytes{{{{'+','.join(f'0x{v:02x}' for v in b)+'}};',f'inline constexpr std::array<std::uint16_t,{len(fixes)}> {key}_relocations{{{{'+','.join(map(str,fixes))+'}};']
 records.append({'name':key,'rva':rva,'bytes':b.hex(),'relocations':fixes})
out+=['inline constexpr Region regions[]={']+[f'{{0x{rva:x},{key}_bytes.data(),{key}_bytes.size(),{key}_relocations.data(),{key}_relocations.size()}},' for key,rva,size in regions if key!='workspace_context']+['};','}']
out[-1:-1]=['inline constexpr Region mesh_body_regions[]={']+[f'{{0x{rva:x},{key}_bytes.data(),{key}_bytes.size(),{key}_relocations.data(),{key}_relocations.size()}},' for key,rva,size in regions if key.startswith('mesh_fast_body') or key.startswith('mesh_legacy_body')]+['};']
out[-1:-1]=['inline constexpr Region mesh_draw_regions[]={']+[f'{{0x{rva:x},{key}_bytes.data(),{key}_bytes.size(),{key}_relocations.data(),{key}_relocations.size()}},' for key,rva,size in regions if key.startswith('mesh_workspace_') or key.startswith('mesh_material_')]+['};']
fleet_path=Path('/home/tamsynn/NVMeData/Fleet Ops Roots/FleetOpsHook.dll')
fleet_data=fleet_path.read_bytes()
assert hashlib.sha256(fleet_data).hexdigest()=='3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11'
fp=PE(fleet_data);frva,fsize=struct.unpack('<II',fp.raw(fp.opt+96+5*8,8));frels=[];at=0
while at<fsize:
 page,size=struct.unpack('<II',fp.read(frva+at,8));assert size>=8
 for offset in range(8,size,2):
  item=struct.unpack('<H',fp.read(frva+at+offset,2))[0]
  if item>>12==3:frels.append(page+(item&0xfff))
 at+=size
rva=0x1cedec;b=fp.read(rva,0x3d);fixes=[v-rva for v in frels if rva<=v<rva+len(b)]
assert all(v+4<=len(b) for v in fixes)
assert struct.unpack('<I',fp.read(0x211e60,4))[0]==fp.base+0x210b90
assert struct.unpack('<I',fp.read(0x210b90,4))[0]==0x62ae80
out.pop() # close Root profile after appending this separate Fleet span
out += [f'inline constexpr std::uint32_t fleet_preferred_base=0x{fp.base:x};',
 'inline constexpr std::uint32_t fleet_constructor_rva=0x1cedec;',
 f'inline constexpr std::array<std::uint8_t,{len(b)}> fleet_constructor_bytes{{{{'+','.join(f'0x{v:02x}' for v in b)+'}};',
 f'inline constexpr std::array<std::uint16_t,{len(fixes)}> fleet_constructor_relocations{{{{'+','.join(map(str,fixes))+'}};',
 'inline constexpr std::array<unsigned char,32> fleet_sha{{'+','.join('0x'+v for v in [hashlib.sha256(fleet_data).hexdigest()[i:i+2] for i in range(0,64,2)])+'}};', '}']
records.append({'name':'Fleet constructor wrapper','rva':rva,'bytes':b.hex(),'relocations':fixes,'image_sha256':hashlib.sha256(fleet_data).hexdigest(),'slot_pointer_rva':0x211e60,'slot_rva':0x210b90,'original_ctor_va':0x62ae80})
Path('core/refit_scene_profile.hpp').write_text('\n'.join(out)+'\n')
p=Path('build/refit-scene-profile');p.mkdir(exist_ok=True);(p/'source-receipt.json').write_text(json.dumps({'images':[{'path':str(p),'sha256':s} for p,(_,s) in zip(paths,images)],'regions':records},indent=2)+'\n')
