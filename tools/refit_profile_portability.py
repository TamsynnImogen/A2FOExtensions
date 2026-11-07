#!/usr/bin/env python3
"""Read-only: check every byte-verified upstream render3d symbol (profiles/gog-1.1/render3d*.toml)
against a target executable, at the same address (fn/label/patch) or by pattern count (scan).
Usage: refit_profile_portability.py EXE [EXE...]  -> JSON summary on stdout. Never patches anything."""
import collections, json, re, struct, sys, tomllib, glob
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1] / 'third_party/armada2-refit-native'
def load(p):
    d = Path(p).read_bytes(); pe = struct.unpack_from('<I', d, 0x3c)[0]; n = struct.unpack_from('<H', d, pe + 6)[0]
    osz = struct.unpack_from('<H', d, pe + 20)[0]; opt = pe + 24; base = struct.unpack_from('<I', d, opt + 28)[0]
    secs = [(d[opt+osz+i*40:opt+osz+i*40+8].rstrip(b'\0').decode(),) + struct.unpack_from('<IIII', d, opt+osz+i*40+8) for i in range(n)]
    return {'data': d, 'base': base, 'secs': secs, 'timestamp': hex(struct.unpack_from('<I', d, pe + 8)[0]),
            'image_size': hex(struct.unpack_from('<I', d, opt + 56)[0]), 'sections': [s[0] for s in secs]}
def v2o(img, va):
    r = va - img['base']
    for nm, vs, va_, rs, ro in img['secs']:
        if va_ <= r < va_ + max(vs, rs) and r - va_ < rs: return ro + r - va_
def main():
    out = {}
    for exe in sys.argv[1:]:
        img = load(exe); res = collections.Counter(); misses = []
        for f in sorted(glob.glob(str(ROOT / 'profiles/gog-1.1/render3d*.toml'))):
            mod = Path(f).stem
            for s in tomllib.load(open(f, 'rb')).get('sym', []):
                chk = s.get('check')
                if not chk: res['unchecked'] += 1; continue
                toks = chk.split(); check = [int(x, 16) if x != '??' else None for x in toks]
                if s.get('kind') == 'scan':
                    pat = b''.join(b'.' if c is None else re.escape(bytes([c])) for c in check); hits = 0
                    for nm, vs, va_, rs, ro in img['secs']:
                        if nm == s.get('section', '.text'): hits += len(re.findall(pat, img['data'][ro:ro + rs], re.S))
                    ok = hits == s.get('count', 1)
                elif 'addr' in s:
                    o = v2o(img, s['addr']); ok = o is not None and all(c is None or img['data'][o + i] == c for i, c in enumerate(check))
                else: res['no-address'] += 1; continue
                res['match' if ok else 'miss'] += 1
                if not ok: misses.append(f"{mod}:{s['id']}")
        out[exe] = {'timestamp': img['timestamp'], 'image_size': img['image_size'], 'sections': img['sections'], 'counts': dict(res), 'misses': misses}
    print(json.dumps(out, indent=2))
if __name__ == '__main__': main()
