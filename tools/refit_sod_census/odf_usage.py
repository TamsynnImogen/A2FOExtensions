#!/usr/bin/env python3
"""Resolve every effective FO4 Nexus ODF (Nexus > Fleet Ops 4.0 > base loose files; ODF.fpq mirrors
the base loose set) with #include inheritance; record classlabel and basename. Read-only.
Usage: odf_usage.py OUT.json"""
import os,re,sys,collections,json
R="/home/tamsynn/NVMeData/Fleet Ops Roots/Data"
roots=[R+'/Mods/FO4 Nexus',R+'/Mods/Fleet Ops 4.0',R]
eff={}
for root in roots:
  for dp,dn,fn in os.walk(root):
    if root==R and '/Mods' in dp[len(R):]+'/' and dp!=R: 
      if dp.startswith(R+'/Mods'):continue
    if '.deployment-backups' in dp:continue
    for n in fn:
      if n.lower().endswith('.odf') and n.lower() not in eff:eff[n.lower()]=os.path.join(dp,n)
kv=re.compile(r'^\s*([A-Za-z0-9_]+)\s*=\s*("([^"]*)"|[^\s/]+)');inc=re.compile(r'^\s*#include\s+"([^"]+)"',re.I)
cache={}
def load(name,depth=0):
  name=name.lower()
  if name in cache:return cache[name]
  if depth>16 or name not in eff:return {}
  cache[name]={}
  out={}
  try:lines=open(eff[name],encoding='latin1').read().splitlines()
  except OSError:return {}
  for l in lines:
    l=l.split('//')[0]
    m=inc.match(l)
    if m:out.update(load(m.group(1),depth+1));continue
    m=kv.match(l)
    if m:out[m.group(1).lower()]=(m.group(3) if m.group(3) is not None else m.group(2))
  cache[name]=out;return out
res={}
for n in eff:res[n]=load(n)
json.dump({'count':len(eff),'odfs':{n:{k:v for k,v in d.items() if k in('classlabel','basename','buildtime','tooltip','unitname','race','buildable')} for n,d in res.items()}},open(sys.argv[1],'w'))
print(len(eff),collections.Counter(d.get('classlabel','<none>').lower() for d in res.values()).most_common(30))
