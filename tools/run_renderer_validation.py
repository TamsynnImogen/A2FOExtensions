#!/usr/bin/env python3
"""Console renderer fixtures in a private headless compositor; no game/input.

Requires an existing dedicated Wine prefix. Never inherits the desktop display.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--fixture', choices=('all','synthetic','device','window','radar','radar-diagnostics','compat','ui','vs11','vs11-cpu','dds','bc','bc-inventory','game','sampler','alpha','cloak','cloak-dispatch'), default='all')
    parser.add_argument('--wrapper-root', type=Path,
                        help='Read-only source of existing d3d8/dxwrapper/d3d9 payloads; stage copies in the test host')
    parser.add_argument('--d3d8-provider-root', type=Path, help='private native D3D8/D3D9 provider copy, no dxwrapper required')
    parser.add_argument('--reference-executable',type=Path,
                        help='radar-diagnostics only: map this EXE as inert data, never run its entry point')
    parser.add_argument('--vs-program', type=Path,
                        help='vs11 only: read-only captured shader bytecode, at most 4096 whole words')
    parser.add_argument('--texture-dds', type=Path, help='bc only: read-only compressed DDS sampling input')
    parser.add_argument('--texture-reference-rgba', type=Path, help='bc only: 256-byte CPU point-sampled RGBA reference; requires texture-dds')
    parser.add_argument('--texture-directory', type=Path, help='bc-inventory only: read-only directory of converted DDS files')
    parser.add_argument('--game-mod-root', type=Path, help='game only: prepared BC child root')
    parser.add_argument('--game-normal-reference', type=Path)
    parser.add_argument('--game-colour-reference', type=Path)
    parser.add_argument('--game-d3dx', type=Path, help='game only: installed D3DX81ab.dll, read only')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if not output.is_relative_to(repo / 'build'):
        parser.error('output must be beneath repository build/')
    prefix = args.prefix.resolve()
    if not prefix.name.startswith('a2fo-') or not (prefix / 'system.reg').is_file():
        parser.error('use an initialized dedicated a2fo-* Wine prefix')
    output.mkdir(parents=True, exist_ok=True)
    if args.reference_executable and (args.fixture!='radar-diagnostics' or not args.reference_executable.is_file()):
        parser.error('reference-executable must be an existing file used only with radar-diagnostics')
    if args.vs_program:
        if args.fixture != 'vs11' or not args.vs_program.is_file():
            parser.error('vs-program must be an existing file used only with vs11')
        size = args.vs_program.stat().st_size
        if size < 8 or size > 4096 * 4 or size % 4:
            parser.error('vs-program must contain 2 to 4096 whole 32-bit words')
    if args.texture_dds and (args.fixture != 'bc' or not args.texture_dds.is_file()):
        parser.error('texture-dds must be an existing file used only with bc')
    if args.texture_reference_rgba and (not args.texture_dds or not args.texture_reference_rgba.is_file() or args.texture_reference_rgba.stat().st_size != 256):
        parser.error('texture-reference-rgba requires texture-dds and an existing 256-byte file')
    if args.fixture == 'bc-inventory' and (not args.texture_directory or not args.texture_directory.is_dir()):
        parser.error('bc-inventory requires an existing texture-directory')
    if args.texture_directory and args.fixture != 'bc-inventory':
        parser.error('texture-directory is only for bc-inventory')
    if args.fixture == 'game':
        if not args.game_mod_root or not (args.game_mod_root / '.a2fo-dx11/catalog.txt').is_file():
            parser.error('game requires a prepared game-mod-root')
        if not all(p and p.is_file() and p.stat().st_size == 256 for p in (args.game_normal_reference, args.game_colour_reference)):
            parser.error('game requires both 256-byte CPU references')
        if not args.game_d3dx or not args.game_d3dx.is_file():
            parser.error('game requires the installed game-d3dx loader DLL')
    elif any((args.game_mod_root, args.game_normal_reference, args.game_colour_reference, args.game_d3dx)):
        parser.error('game arguments are only for game fixture')
    if args.fixture == 'alpha' and not args.d3d8_provider_root:
        parser.error('alpha requires --d3d8-provider-root for the installed native provider')
    env = dict(os.environ)
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'STEAM_COMPAT_DATA_PATH', 'LD_PRELOAD'):
        env.pop(name, None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
               WINEDLLOVERRIDES='d3d8=b;winemenubuilder.exe=d')
    vs_program_windows = None
    if args.vs_program:
        translated = subprocess.run(['winepath', '-w', str(args.vs_program.resolve())],
                                    env=env, check=True, capture_output=True, text=True)
        vs_program_windows = translated.stdout.strip()
        if not vs_program_windows:
            parser.error('winepath returned no Windows path for vs-program')
    fixtures = [('d3d8_observer_test.exe', 'PASS: D3D8 observer', False),
                ('d3d11_probe_native.exe', 'PASS: D3D11 probe lifetime and present', True),
                ('d3d8_d3d11_window_test.exe', 'PASS: real D3D8 factory and resources', True)]
    if args.fixture == 'radar':
        fixtures = [('radar_clear_native.exe', 'PASS: minimap Clear rectangles', True)]
    elif args.fixture == 'radar-diagnostics':
        fixtures = [('radar_diagnostics_test.exe', 'PASS: radar diagnostics', False)]
    elif args.fixture == 'compat':
        fixtures = [('renderer_fvf_test.exe','PASS: renderer FVF',False),
                    ('renderer_texture_upload_test.exe','PASS: D3D8 texture upload conversion',False),
                    ('renderer_shader_cache_test.exe','PASS: SM5 compile, cache hit',False),
                    ('renderer_shader_tokens_test.exe','PASS: bounded D3D8 declaration and shader token boundaries',False)]
    elif args.fixture == 'ui':
        fixtures = [('renderer_ui_native.exe','PASS: D3D8 and D3D11 UI pixels',True)]
    elif args.fixture == 'vs11':
        fixtures = [('renderer_vs11_native.exe','PASS: captured VS1.1 D3D8/D3D11 pixels',True)]
    elif args.fixture == 'vs11-cpu':
        fixtures = [('renderer_vs11_test.exe','PASS: bounded VS1.1 translation',False)]
    elif args.fixture == 'dds':
        fixtures = [('renderer_dds_test.exe','PASS: compressed DDS parsing, layout and validation',False)]
    elif args.fixture == 'bc':
        fixtures = [('renderer_bc_native.exe','PASS: BC5 BC7 compressed DDS GPU sampling',True)]
    elif args.fixture == 'bc-inventory':
        fixtures = [('renderer_bc_inventory.exe','PASS: all BC DDS GPU resources',True)]
    elif args.fixture == 'sampler':
        fixtures = [('renderer_sampler_native.exe','PASS: native D3D8 sampler probe',True)]
    elif args.fixture == 'alpha':
        fixtures = [('renderer_alpha_native.exe','PASS: paired native D3D8 alpha stage pixels',True)]
    elif args.fixture == 'cloak':
        fixtures = [('renderer_cloak_native.exe','PASS: Refit cloak refraction',True)]
    elif args.fixture == 'cloak-dispatch':
        fixtures = [('renderer_cloak_dispatch_native.exe','PASS: cloak native dispatch',False)]
    elif args.fixture == 'game':
        fixtures = [('renderer_game_native.exe','PASS: translated D3D8 game native fixture',True)]
    elif args.fixture != 'all':
        fixtures = [fixtures[('synthetic','device','window').index(args.fixture)]]
    if args.wrapper_root and args.d3d8_provider_root:
        parser.error('choose one provider source')
    provider_names = ('d3d8.dll','d3d9.dll') if args.d3d8_provider_root else ('d3d8.dll','dxwrapper.dll','d3d9.dll','dxwrapper.ini')
    if args.wrapper_root or args.d3d8_provider_root:
        import shutil
        source = (args.wrapper_root or args.d3d8_provider_root).resolve()
        if output.is_relative_to(source) or source.is_relative_to(output):
            parser.error('private output and wrapper source must not overlap')
        for name in provider_names:
            if not (source / name).is_file():
                parser.error(f'wrapper source missing {name}')
            shutil.copy2(source / name, output / name)
        env['WINEDLLOVERRIDES'] = 'd3d8=n,b;d3d9=n,b;winemenubuilder.exe=d'
        # Installed settings remain unchanged in the private copy too.
        env['DXVK_LOG_PATH'] = str(output)
        env['DXVK_STATE_CACHE_PATH'] = str(output)
    results = []
    for filename, marker, gpu in fixtures:
        executable = repo / 'build' / filename
        if not executable.is_file():
            parser.error(f'build {executable} first')
        if args.wrapper_root or args.d3d8_provider_root:
            import shutil
            staged = output / filename
            shutil.copy2(executable, staged)
            executable = staged
        log_path = output / (Path(filename).stem + '.log')
        with tempfile.TemporaryDirectory(prefix='a2fo-renderer-runtime-') as runtime:
            run_env = dict(env, XDG_RUNTIME_DIR=runtime)
            command = ['wine', str(executable), str(output)]
            if args.fixture == 'cloak':
                # Wine's desktop clients can keep gamescope alive after the
                # console process exits. Record the actual Wine child result;
                # a PASS printed before a teardown crash is insufficient.
                command = ['python3', '-c',
                           'import subprocess,sys; r=subprocess.run(sys.argv[1:]); '
                           'print("A2FO_CLOAK_PROCESS_EXIT="+str(r.returncode),flush=True); '
                           'sys.exit(r.returncode)', *command]
            if vs_program_windows:
                command += [vs_program_windows]
            if args.fixture == 'game':
                command += [str(args.game_mod_root.resolve()), str(args.game_normal_reference.resolve()), str(args.game_colour_reference.resolve()), str(args.game_d3dx.resolve())]
            if args.texture_directory:
                command += [str(args.texture_directory.resolve())]
            if args.texture_dds:
                command += [str(args.texture_dds.resolve())]
                if args.texture_reference_rgba:
                    command += [str(args.texture_reference_rgba.resolve())]
            if args.reference_executable:
                command += [str(args.reference_executable.resolve()),str(repo/'build/modules/A2FORadarDiagnostics.dll')]
            if gpu:
                command = ['gamescope', '--backend', 'headless', '-W', '128', '-H', '128', '--',
                           'env', '-u', 'WAYLAND_DISPLAY', *command]
            with log_path.open('w') as log:
                process = subprocess.Popen(command, cwd=output, env=run_env, stdout=log,
                                           stderr=subprocess.STDOUT, start_new_session=True)
                deadline = time.monotonic() + 90
                status = 'FAIL'
                try:
                    while time.monotonic() < deadline:
                        text = log_path.read_text(errors='replace')
                        if args.fixture == 'cloak' and 'A2FO_CLOAK_PROCESS_EXIT=' in text:
                            if marker in text and 'A2FO_CLOAK_PROCESS_EXIT=0\n' in text:
                                status = 'PASS'
                            break
                        if marker in text and args.fixture != 'cloak':
                            status = 'PASS'
                            break
                        if 'SKIP:' in text:
                            status = 'SKIP'
                            break
                        if 'FAIL:' in text:
                            break
                        if process.poll() is not None:
                            if args.fixture == 'cloak' and marker in text and process.returncode == 0:
                                status = 'PASS'
                            break
                        time.sleep(0.2)
                finally:
                    if process.poll() is None:
                        # Only the private fixture/compositor process group.
                        os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait(timeout=10)
        print(f'{status}: {filename}; {log_path}', flush=True)
        results.append(dict(fixture=filename, status=status, log=str(log_path),
                            fixture_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                            texture_directory=str(args.texture_directory.resolve()) if args.texture_directory else None,
                            game_d3dx_sha256=hashlib.sha256(args.game_d3dx.read_bytes()).hexdigest() if args.game_d3dx else None,
                            game_mod_root=str(args.game_mod_root.resolve()) if args.game_mod_root else None,
                            game_normal_reference_sha256=hashlib.sha256(args.game_normal_reference.read_bytes()).hexdigest() if args.game_normal_reference else None,
                            game_colour_reference_sha256=hashlib.sha256(args.game_colour_reference.read_bytes()).hexdigest() if args.game_colour_reference else None,
                            texture_reference_rgba_sha256=hashlib.sha256(args.texture_reference_rgba.read_bytes()).hexdigest() if args.texture_reference_rgba else None,
                            texture_dds_sha256=hashlib.sha256(args.texture_dds.read_bytes()).hexdigest() if args.texture_dds else None,
                            reference_sha256=hashlib.sha256(args.reference_executable.read_bytes()).hexdigest() if args.reference_executable else None,
                            vs_program_sha256=hashlib.sha256(args.vs_program.read_bytes()).hexdigest() if args.vs_program else None,
                            diagnostic_dll_sha256=hashlib.sha256((repo/'build/modules/A2FORadarDiagnostics.dll').read_bytes()).hexdigest() if args.reference_executable else None,
                            wrapper_sha256={name:hashlib.sha256((output/name).read_bytes()).hexdigest()
                                            for name in provider_names}
                            if (args.wrapper_root or args.d3d8_provider_root) else {}))
    (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    return 0 if all(row['status'] == 'PASS' for row in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
