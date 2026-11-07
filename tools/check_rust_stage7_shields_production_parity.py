#!/usr/bin/env python3
"""Compare shields against the frozen original using production GNU++17 arithmetic."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UNIT = Path('modules/A2FODirectionalShields/directional_shields.cpp')
HEADER = UNIT.with_suffix('.hpp')
FIXTURE = ROOT / 'tests/rust_stage6_shields_parity.cpp'
HASHES = ROOT / 'build/rust-migration-stage6/before-hashes.json'


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, env=None):
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        process = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                 stderr=stderr, timeout=180)
        stdout.seek(0)
        stderr.seek(0)
        output, errors = stdout.read(), stderr.read()
    if process.returncode:
        raise RuntimeError(f'exit {process.returncode}: {command}\n'
                           f'{output[-1500:]!r}\n{errors[-3500:]!r}')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--baseline', type=Path,
                        default=ROOT / 'build/rust-migration-stage6/before')
    parser.add_argument('--wine-prefix', type=Path,
                        default=ROOT / 'build/rust-migration-stage6-wine')
    parser.add_argument('--receipt', type=Path)
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    hashes = json.loads(HASHES.read_text())
    for path in (UNIT, HEADER):
        expected = hashes[str(path)]
        actual = sha256(baseline / path)
        if actual != expected:
            parser.error(f'frozen {path} hash mismatch: {actual} != {expected}')
    if sha256(ROOT / HEADER) != hashes[str(HEADER)]:
        parser.error(f'public header {HEADER} changed since freeze')

    target = 'i686-pc-windows-gnu/release' if args.windows else 'release'
    archive = ROOT / 'build/rust' / target / 'liba2fo_core.a'
    inputs = [*(ROOT / 'rust/a2fo_core/src').glob('*.rs'),
              ROOT / 'rust/a2fo_core/Cargo.toml', ROOT / 'rust/a2fo_core/Cargo.lock']
    if not archive.is_file() or any(not x.is_file() or
                                    x.stat().st_mtime_ns > archive.stat().st_mtime_ns
                                    for x in inputs):
        parser.error(f'{target} archive is missing or stale; build it after source changes')
    archive_hash = sha256(archive)
    env = os.environ.copy()
    runner = []
    if args.windows:
        for key in ('DISPLAY', 'WAYLAND_DISPLAY'):
            env.pop(key, None)
        env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                    'winewayland.drv=d;winex11.drv=d')
        runner = ['wine']

    with tempfile.TemporaryDirectory(prefix='a2fo-rust6-shields-') as temp:
        outputs = []
        for before in (True, False):
            base = baseline if before else ROOT
            exe = Path(temp) / (('before' if before else 'rust') +
                                ('.exe' if args.windows else ''))
            command = ['i686-w64-mingw32-g++' if args.windows else 'g++',
                       '-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
                       '-Werror', '-I', str(base / UNIT.parent), str(FIXTURE),
                       str(base / UNIT)]
            if args.windows:
                command.append('-static')
            if not before:
                command.append(str(archive))
            command += ['-o', str(exe)]
            run(command)
            outputs.append(run(runner + [str(exe)], env))
    original, migrated = outputs
    if original != migrated:
        a, b = original.splitlines(), migrated.splitlines()
        for i in range(max(len(a), len(b))):
            x = a[i] if i < len(a) else b'<missing>'
            y = b[i] if i < len(b) else b'<missing>'
            if x != y:
                print(f'differs at line {i + 1}\nbefore: {x[:1200]!r}\n'
                      f'rust:   {y[:1200]!r}', file=sys.stderr)
                return 1
    if sha256(archive) != archive_hash:
        raise RuntimeError('Rust archive changed during comparison; rerun after build finishes')
    receipt = {'windows': args.windows, 'archive_sha256': archive_hash,
               'fixture_sha256': sha256(FIXTURE), 'before_cpp_sha256': sha256(baseline / UNIT),
               'before_header_sha256': sha256(baseline / HEADER),
               'current_cpp_sha256': sha256(ROOT / UNIT),
               'cases': len(migrated.splitlines()), 'sha256': hashlib.sha256(migrated).hexdigest(),
               'equal': True}
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f"shields: {receipt['cases']} traces, {receipt['sha256']}, PASS", flush=True)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        sys.exit(2)
