#!/usr/bin/env python3
"""Colour parity from a material-colour control directory (run-controls-v2 output).

For each backend and fixture ROI, compares the modern renderer with authored colours
('on') and without them ('off') against the game's own drawing ('native', ground truth).
Only pixels belonging to the ship (any capture differs from the darkest-of-three by more
than a background threshold) are scored. Reports mean absolute RGB error and the share
of ship pixels within 8/255. Read-only. Usage: refit_colour_parity.py CONTROL_DIR [OUT.json]
"""
import json, sys
from pathlib import Path
from PIL import Image

ROIS = {'probe (refit_colour_team)': (560, 440, 720, 580), 'receiver (refit_colour_receiver)': (1030, 420, 1160, 600)}

def load(d, backend, mode):
    return Image.open(d / f'{backend}-{mode}' / 'private-root.png').convert('RGB')

def score(on, off, native, box):
    a, b, n = (list(i.crop(box).getdata()) for i in (on, off, native))
    ship = [k for k in range(len(n)) if max(max(a[k]), max(b[k]), max(n[k])) > 24]
    if not ship: return None
    def err(x):
        e = [sum(abs(x[k][c] - n[k][c]) for c in range(3)) / 3 for k in ship]
        return {'mean_abs': round(sum(e) / len(e), 2), 'within_8': round(sum(1 for v in e if v <= 8) / len(e), 3)}
    return {'ship_pixels': len(ship), 'on_vs_native': err(a), 'off_vs_native': err(b)}

def main():
    d = Path(sys.argv[1]); out = {'control_dir': str(d), 'results': {}}
    for backend in ('dx12', 'vulkan'):
        try: on, off, native = (load(d, backend, m) for m in ('on', 'off', 'native'))
        except FileNotFoundError: continue
        out['results'][backend] = {name: score(on, off, native, box) for name, box in ROIS.items()}
    text = json.dumps(out, indent=2); print(text)
    if len(sys.argv) > 2: Path(sys.argv[2]).write_text(text + '\n')

if __name__ == '__main__': main()
