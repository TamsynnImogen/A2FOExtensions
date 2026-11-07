#!/usr/bin/env python3
"""Stage a separate, hash-verified Bink-free Roots trial. Never changes the live game."""
import argparse
import json
import os
from pathlib import Path
import shutil
from prepare_legacy_dll_independence import canonical,inventory,sha256,REPO
from check_rust_media_contract import Image
from retarget_bink_imports import KNOWN,rewrite

BINK_SHA="9c2b75defd3bf13737fdb6e8b7cd3b67d7b2d6fe0bbf48ec6dc2ecf1b03e559b"
BASE="build/legacy-dll-independence-20261030/test-installation"

def prepare(source,output,core,proxy):
    source=canonical(source);output=canonical(output,missing=True)
    core=canonical(core);proxy=canonical(proxy)
    if source!=canonical(REPO/BASE):raise ValueError("only the accepted selected-mod trial is supported")
    if not output.is_relative_to(REPO/"build") or output==REPO/"build" or output.exists():raise ValueError("output must be fresh under build/")
    if output.is_relative_to(source) or source.is_relative_to(output):raise ValueError("source/output overlap")
    quarantine=output.with_name(output.name+".quarantine")
    receipt=output.with_name(output.name+".receipt.json")
    if quarantine.exists() or receipt.exists():raise ValueError("quarantine/receipt already exists")
    original=Image(source/"Data/A2FOExtensions.dll");candidate=Image(core)
    if len(candidate.exports)!=210 or candidate.exports[:198]!=original.exports:raise ValueError("old 198 exports changed")
    if any("bink" in s or "avcodec" in s or "avformat" in s for s in candidate.imports):raise ValueError("native codec import")
    if Image(proxy).exports!=Image(source/"Data/Win2kDisableTaskSwitch.dll").exports:raise ValueError("startup exports changed")
    before=inventory(source)
    binks=[r for r,h in before.items() if Path(r).name.lower()=="binkw32.dll"]
    if len(binks)!=3 or any(before[r]!=BINK_SHA for r in binks):raise ValueError("unexpected Bink inventory")
    core_sha,proxy_sha=sha256(core),sha256(proxy)
    size=sum((source/r).stat().st_size for r in before)+256*1024*1024
    disk=output.parent
    while not disk.exists():disk=disk.parent
    if shutil.disk_usage(disk).free<size:raise ValueError("insufficient staging space")
    record=dict(status="staging",source=str(source),output=str(output),quarantine=str(quarantine),source_before=before,
                core=dict(path=str(core),sha256=core_sha),proxy=dict(path=str(proxy),sha256=proxy_sha),replaced={},retired={},
                in_game_acceptance="pending",ready_for_manual_launch=False)
    output.parent.mkdir(parents=True,exist_ok=True);quarantine.mkdir()
    try:
        shutil.copytree(source,output,copy_function=shutil.copy2)
        if inventory(output)!=before:raise ValueError("copied tree differs")
        def retire(relative,category):
            old=output/relative;destination=quarantine/category/relative
            destination.parent.mkdir(parents=True,exist_ok=True);old.rename(destination)
            if sha256(destination)!=before[relative]:raise ValueError("quarantine hash mismatch")
            record["retired" if category=="bink" else "replaced"][relative]=dict(original_sha256=before[relative],backup=str(destination))
        replacements={"Data/A2FOExtensions.dll":core.read_bytes(),"Data/Win2kDisableTaskSwitch.dll":proxy.read_bytes()}
        for relative in ["FleetOpsHook.dll","Data/ArmadaL.exe","Data/Mods/Fleet Ops 4.0/ArmadaL.exe"]:
            changed,detail=rewrite((source/relative).read_bytes());replacements[relative]=changed
            record.setdefault("retargeted",{})[relative]=detail
        for relative in binks:retire(relative,"bink")
        for relative,data in replacements.items():
            retire(relative,"superseded");(output/relative).write_bytes(data)
            shutil.copystat(source/relative,output/relative,follow_symlinks=False)
            record["replaced"][relative]["staged_sha256"]=sha256(output/relative)
        # Exact fingerprints of the reviewed private image mutations. No code
        # addresses, thunk locations or section geometry are relaxed.
        from armada_compat_profile import parse_image,make_profile,make_fleetops_profile
        profiles={"Data/A2FOArmadaProfiles.txt":make_profile(parse_image(replacements["Data/ArmadaL.exe"])),
                  "A2FOFleetOpsProfiles.txt":make_fleetops_profile(parse_image(replacements["FleetOpsHook.dll"]))}
        for relative,text in profiles.items():
            if relative in before:retire(relative,"superseded")
            (output/relative).write_text(text,encoding="ascii")
            record.setdefault("profiles",{})[relative]=sha256(output/relative)
        after=inventory(source)
        if before!=after:raise ValueError("source changed during staging")
        staged=inventory(output)
        if any(Path(r).name.lower()=="binkw32.dll" for r in staged):raise ValueError("Bink remains inside trial")
        for relative,pin in before.items():
            if relative not in replacements and relative not in profiles and relative not in binks and staged.get(relative)!=pin:
                raise ValueError("unplanned staged change: "+relative)
        consumers=[]
        for relative in staged:
            if Path(relative).suffix.lower() not in (".dll",".exe"):continue
            data=(output/relative).read_bytes()
            # The selected install also contains four unrelated x64 helpers.
            import struct
            if data[:2]!=b"MZ":raise ValueError("non-PE executable: "+relative)
            pe=struct.unpack_from("<I",data,0x3c)[0]
            if struct.unpack_from("<H",data,pe+4)[0]!=0x14c:continue
            im=Image(output/relative)
            if "binkw32.dll" in im.imports:consumers.append(relative)
        if consumers:raise ValueError("Bink normal imports remain: "+repr(consumers))
        if sha256(core)!=core_sha or sha256(proxy)!=proxy_sha:raise ValueError("supplied image changed")
        record.update(status="staged_headless_validation_pending",source_after=after,staged=staged,normal_bink_imports=[],
                      note="Launch Data/ArmadaL.exe from Data. Selected fo test/test mods only; other variants are not included.")
    except Exception as e:
        record.update(status="failed",error=str(e));raise
    finally:
        with receipt.open("x") as f:json.dump(record,f,indent=2);f.write("\n")
    return record

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-install",type=Path,default=REPO/BASE);p.add_argument("--output",type=Path,required=True)
    p.add_argument("--core",type=Path,required=True);p.add_argument("--proxy",type=Path,required=True)
    a=p.parse_args();r=prepare(a.source_install,a.output,a.core,a.proxy)
    print(json.dumps({k:r[k] for k in ["status","output","quarantine","normal_bink_imports","in_game_acceptance"]},indent=2))
if __name__=="__main__":main()
