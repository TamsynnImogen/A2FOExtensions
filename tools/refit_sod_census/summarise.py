#!/usr/bin/env python3
"""Summarise refit_sod_census JSONL into coverage.json + REPORT.md (read-only)."""
import collections, json, sys
from pathlib import Path

src = Path(sys.argv[1]); out = src.parent
rows = [json.loads(l) for l in src.read_text().splitlines() if l.strip()]
meta = rows[0]; files = [r for r in rows[1:] if r['kind'] == 'file']
GATES = ['sod_version', 'parser_policy', 'multi_group', 'triangle_limit', 'blend_mode']

def static_blockers(f, m):
    """Every static gate the mesh fails (not just the first)."""
    b = []
    if not f['version_ok']: b.append('sod_version')
    if not f['policy_ok']: b.append('parser_policy')
    if m['groups'] != 1: b.append('multi_group')
    if m['tris'] > 21845: b.append('triangle_limit')
    if m['mode'] not in ('opaque', 'default'): b.append('blend_mode')
    return b

meshes = [(f, m) for f in files for m in f.get('meshes', [])]
tot_n = len(meshes); tot_t = sum(m['tris'] for _, m in meshes)
pct = lambda a, b: round(100.0 * a / b, 1) if b else 0.0

ver = collections.Counter(str(f.get('version')) for f in files)
first = collections.Counter(); first_t = collections.Counter()
for f, m in meshes:
    first[m['blocker'] or 'eligible'] += 1; first_t[m['blocker'] or 'eligible'] += m['tris']

# "What if" — lift one or more gates, how much becomes statically eligible.
def eligible_if(lifted):
    n = t = 0
    for f, m in meshes:
        if all(g in lifted for g in static_blockers(f, m)): n += 1; t += m['tris']
    return n, t
whatif = {}
for combo in (['multi_group'], ['blend_mode'], ['multi_group', 'blend_mode'],
              ['multi_group', 'blend_mode', 'triangle_limit'], ['multi_group', 'blend_mode', 'parser_policy'],
              ['multi_group', 'blend_mode', 'triangle_limit', 'parser_policy', 'sod_version']):
    n, t = eligible_if(set(combo))
    whatif['+'.join(combo)] = {'meshes': n, 'mesh_pct': pct(n, tot_n), 'tris': t, 'tri_pct': pct(t, tot_t)}

elig = [(f, m) for f, m in meshes if not m['blocker']]
def share(pred, pool):
    n = sum(1 for f, m in pool if pred(m)); t = sum(m['tris'] for f, m in pool if pred(m))
    return {'meshes': n, 'tris': t, 'mesh_pct_of_pool': pct(n, len(pool)),
            'tri_pct_of_pool': pct(t, sum(m['tris'] for _, m in pool))}
eligible_traits = {
    'bump_dot3_path': share(lambda m: m['bump'], elig),
    'self_illuminated': share(lambda m: m['self_illum_groups'] > 0, elig),
    'specular_material': share(lambda m: m['specular'], elig),
    'animated_node_or_ancestor': share(lambda m: m['animated'], elig),
    'under_damage_node': share(lambda m: m['damage'], elig),
    'lower_lod_level': share(lambda m: m['lod'] and m['lod'][0] < m['lod'][1] - 1, elig),
    'two_sided': share(lambda m: m['cull'] == 0, elig),
}
groups_hist = collections.Counter(m['groups'] for _, m in meshes if m['groups'] != 1)
modes = collections.Counter(m['mode'] for _, m in meshes)
modes_t = collections.Counter()
for _, m in meshes: modes_t[m['mode']] += m['tris']

per_model = collections.Counter()
for f in files:
    ms = f.get('meshes', [])
    if not ms: per_model['no_mesh_geometry' if f.get('parse_ok') else 'unparsed'] += 1
    elif all(not m['blocker'] for m in ms): per_model['all_meshes_eligible'] += 1
    elif any(not m['blocker'] for m in ms): per_model['partially_eligible'] += 1
    else: per_model['no_mesh_eligible'] += 1

# Prefix families (race/kind) by triangle coverage, top 15 by triangles.
fam = collections.defaultdict(lambda: [0, 0])
for f, m in meshes:
    p = f['name'].split('_')[0] if '_' in f['name'] else f['name'].rsplit('.', 1)[0]
    fam[p][0] += m['tris']; fam[p][1] += 0 if m['blocker'] else m['tris']
families = sorted(({'prefix': k, 'tris': v[0], 'eligible_tri_pct': pct(v[1], v[0])} for k, v in fam.items()),
                  key=lambda e: -e['tris'])[:15]

parse_fail = [f['name'] for f in files if not f.get('parse_ok')]
policy_fail = [f['name'] for f in files if f.get('parse_ok') and not f['policy_ok']]
nonver = [f"{f['name']} ({f.get('version')})" for f in files if not f['version_ok']]
emitters = sum(f.get('nodes', {}).get('emitter', 0) for f in files)
sprites = sum(f.get('nodes', {}).get('sprite', 0) for f in files)
lights = sum(f.get('nodes', {}).get('light', 0) for f in files)

cov = {
    'scope': 'Static SOD gates only (mesh_scope.cpp + a2fo_native11_model policy). Runtime gates '
             '(legacy route 1 vs MeshVB, pass 0, live pose/visibility, canonical material/texture '
             'rechecks, geometry parity, LOD/damage selection) are NOT evaluated. An upper bound.',
    'meta': meta, 'files': len(files), 'versions': dict(ver), 'mesh_nodes': tot_n, 'triangles': tot_t,
    'first_blocker': {k: {'meshes': first[k], 'tris': first_t[k], 'mesh_pct': pct(first[k], tot_n),
                          'tri_pct': pct(first_t[k], tot_t)} for k in first},
    'what_if_lifted': whatif, 'eligible_traits': eligible_traits,
    'multi_group_histogram': dict(sorted(groups_hist.items())),
    'blend_modes': {k: {'meshes': modes[k], 'tris': modes_t[k]} for k in modes},
    'per_model': dict(per_model), 'families': families,
    'non_mesh_nodes': {'sprite': sprites, 'emitter': emitters, 'light': lights},
    'parse_fail': parse_fail, 'policy_fail': policy_fail, 'version_out_of_range': nonver,
}
(out / 'coverage.json').write_text(json.dumps(cov, indent=1) + '\n')

L = []; w = L.append
w('# Refit replacement coverage census (static)\n')
w(f"Source: `{src}`. {len(files)} effective SODs ({meta['shadowed']} lower-priority files shadowed), "
  f"{tot_n} drawable mesh nodes, {tot_t:,} triangles.\n")
w('> ' + cov['scope'] + '\n')
w('## First blocking static gate\n\n| Result | Meshes | % | Triangles | % |\n|---|---:|---:|---:|---:|')
for k in ['eligible'] + GATES:
    if k in first: w(f"| {k} | {first[k]} | {pct(first[k],tot_n)} | {first_t[k]:,} | {pct(first_t[k],tot_t)} |")
w('\n## If gates were lifted\n\n| Lifted | Meshes % | Triangles % |\n|---|---:|---:|')
for k, v in whatif.items(): w(f"| {k} | {v['mesh_pct']} | {v['tri_pct']} |")
w('\n## Traits of currently eligible meshes (share of eligible pool)\n\n| Trait | Meshes % | Triangles % |\n|---|---:|---:|')
for k, v in eligible_traits.items(): w(f"| {k} | {v['mesh_pct_of_pool']} | {v['tri_pct_of_pool']} |")
w('\n## Per model\n'); [w(f"- {k}: {v}") for k, v in per_model.most_common()]
w('\n## Blend modes (all meshes)\n'); [w(f"- `{k}`: {modes[k]} meshes, {modes_t[k]:,} tris") for k, _ in modes.most_common()]
w('\n## Group counts on multi-group meshes\n'); w(', '.join(f'{k}: {v}' for k, v in sorted(groups_hist.items())))
w('\n## Largest families by triangles\n\n| Prefix | Triangles | Eligible tris % |\n|---|---:|---:|')
for e in families: w(f"| {e['prefix']} | {e['tris']:,} | {e['eligible_tri_pct']} |")
w(f"\n## Non-mesh nodes (never replaced by the per-mesh API)\n\nsprites {sprites}, emitters {emitters}, SOD lights {lights}\n")
w(f"## Rejected files\n\n- parse failures ({len(parse_fail)}): {', '.join(parse_fail[:20]) or 'none'}")
w(f"- parser-policy rejections ({len(policy_fail)}): {', '.join(policy_fail[:30]) or 'none'}")
w(f"- version outside 1.7–1.93 ({len(nonver)}): {', '.join(nonver[:30]) or 'none'}")
(out / 'REPORT.md').write_text('\n'.join(L) + '\n')
print(out / 'REPORT.md')
