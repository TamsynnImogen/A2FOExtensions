#!/usr/bin/env python3
"""Hash-gated private Bink import redirection; retains every thunk and section extent."""
import argparse
import json
import struct
from pathlib import Path
from retarget_imagehlp import BytesImage, checksum, normal_imports, sha, canonical, ROOT
from armada_compat_profile import parse_image, make_profile, make_fleetops_profile

KNOWN = {
    "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de": ("roots-oct23", ".idata",1),
    "41ee2e6dd877b03fdf2b48eb268fbab28c521b5ad02ac2f688856d7d7eefdaf2": ("roots-dbghelp", ".idata",1),
    "c8846226048931134e07d48702309b2fec1ef0f67f5b4d82bec0eb5249257d8c": ("fo4-parent", ".idata",1),
    "370a93632530fa19d0ba14e522e118c637d5611bf406e72cad226fb68b26fb45": ("fo4-parent-dbghelp", ".idata",1),
    "3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11": ("roots-fleetops", ".edata",2),
}
ALLOWED = {"_BinkOpen@8","_BinkClose@4","_BinkDoFrame@4","_BinkNextFrame@4","_BinkWait@4",
           "_BinkCopyToBuffer@28","_BinkGoto@12","_BinkSetVolume@8","_BinkSetSoundSystem@8","_BinkOpenDirectSound@4"}
NEW = b"A2FOExtensions.dll\0"

def rewrite(data):
    digest = sha(data)
    if digest not in KNOWN: raise ValueError(f"unknown image {digest}")
    kind, section_name, count = KNOWN[digest]
    info = parse_image(data)
    im = BytesImage(data)
    imports = normal_imports(data)
    selected = [i for i,x in enumerate(imports) if x["dll"].lower()=="binkw32.dll"]
    if len(selected)!=count: raise ValueError("unexpected Bink descriptor count")
    if any(x["dll"].lower()=="a2foextensions.dll" for x in imports): raise ValueError("already imports replacement")
    for i in selected:
        if not imports[i]["entries"] or any(x.get("name") not in ALLOWED for x in imports[i]["entries"]):
            raise ValueError("unsupported Bink import")
    section = next(x for x in info["sections"] if x["name"]==section_name)
    # These pinned images have backed raw padding in the same mapped page.
    # Do not enlarge VirtualSize or relax the engine's audited ABI geometry.
    displacement = (section["virtual_size"]+3)&~3
    if displacement+len(NEW)>section["raw_size"] or displacement//4096 != (section["virtual_size"]-1)//4096:
        raise ValueError("name does not fit existing mapped-page raw padding")
    offset = section["raw_offset"]+displacement
    if any(data[offset:offset+len(NEW)]): raise ValueError("padding is not empty")
    rva = section["virtual_address"]+displacement
    out = bytearray(data); out[offset:offset+len(NEW)] = NEW
    allowed = set(range(offset,offset+len(NEW)))
    table = im.offset(im.u32(im.optional+104))
    for i in selected:
        at = table+i*20+12
        struct.pack_into("<I",out,at,rva);allowed.update(range(at,at+4))
    field=im.optional+64
    if kind=="roots-fleetops":
        # This pinned DLL's madExcept.CheckFileCrc sets BL on *unequal*
        # checksums and skips its error path when BL is set (RVA 6c2f5..6c312).
        # Its original nonmatching checksum is part of the working baseline.
        # Recalculating it enters a broken error path and faults at RVA 6c3af.
        # Preserve that header field; do not alter executable checker code.
        stored=im.u32(field)
        if not stored or checksum(data,field)==stored or checksum(out,field)==stored:
            raise ValueError("unexpected FleetOpsHook legacy checksum-check state")
        checksum_policy="preserve_pinned_fleetops_legacy_field"
    else:
        struct.pack_into("<I",out,field,checksum(out,field));allowed.update(range(field,field+4))
        checksum_policy="recalculate_engine_pe_checksum"
    out=bytes(out)
    expected=[dict(x,dll="A2FOExtensions.dll",name_offset=offset) if i in selected else x for i,x in enumerate(imports)]
    if normal_imports(out)!=expected: raise ValueError("unexpected import/thunk mutation")
    differences=[i for i,(a,b) in enumerate(zip(data,out)) if a!=b]
    if len(out)!=len(data) or not set(differences)<=allowed: raise ValueError("unexpected byte mutation")
    after=parse_image(out)
    for before_s,after_s in zip(info["sections"],after["sections"]):
        if {k:v for k,v in before_s.items() if k.endswith("crc32")} == {k:v for k,v in after_s.items() if k.endswith("crc32")}:
            continue
        if before_s["name"] not in (section_name,".idata"): raise ValueError("non-import section changed")
    return out,dict(kind=kind,before_sha256=digest,after_sha256=sha(out),changed_file_offsets=differences,
        replacement_name_rva=rva,replacement_name_offset=offset,descriptor_count=count,
        checksum_policy=checksum_policy,stored_checksum_before=im.u32(field),
        stored_checksum_after=BytesImage(out).u32(field),computed_checksum_after=checksum(out,field),
        section_geometry_unchanged=True,executable_sections_unchanged=True,thunks_unchanged=True,
        status="private_import_retarget_runtime_pending",padding_loader_validation_required=True)

def patch(source,output):
    source,output=canonical(source),canonical(output)
    if not output.is_relative_to(ROOT/"build") or output.exists(): raise ValueError("output must be fresh under repository build/")
    data=source.read_bytes(); out,record=rewrite(data)
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("xb") as f:f.write(out)
    if sha(source.read_bytes())!=sha(data):
        output.unlink();raise ValueError("source changed during patch")
    info=parse_image(out)
    if record["kind"]=="roots-fleetops": profile=make_fleetops_profile(info)
    elif record["kind"].startswith("roots"): profile=make_profile(info)
    else:profile=None
    if profile:output.with_name(output.name+".profile.txt").write_text(profile)
    record.update(source=str(source),output=str(output))
    output.with_name(output.name+".receipt.json").write_text(json.dumps(record,indent=2)+"\n")
    return record

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--source",type=Path,required=True);p.add_argument("--output",type=Path,required=True)
    args=p.parse_args();r=patch(args.source,args.output);print(json.dumps({k:v for k,v in r.items() if k!="changed_file_offsets"},indent=2))
if __name__=="__main__":main()
