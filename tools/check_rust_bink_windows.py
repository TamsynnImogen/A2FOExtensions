#!/usr/bin/env python3
"""Run the frozen Windows Bink scanner under Wine and compare Linux receipts."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


TIMEOUT = 300
MAX_JOBS = 2
METADATA_KEYS = (
    'signature', 'width', 'height', 'chroma_width', 'chroma_height',
    'fps_num', 'fps_den', 'source_frame_count', 'decoded_frame_count',
    'has_alpha', 'audio_tracks_in_header', 'audio_sample_rate',
    'audio_channels', 'audio_sample_frames', 'source_bytes', 'video_bytes',
    'audio_bytes', 'alpha_bytes', 'alpha_sha256')


class ProcessFailure(RuntimeError):
    def __init__(self, message, stdout=b'', stderr=b''):
        super().__init__(message)
        self.stdout = stdout or b''
        self.stderr = stderr or b''


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def wine_path(path):
    return 'Z:' + str(Path(path).resolve()).replace('/', '\\')


def wine_env(prefix):
    env = os.environ.copy()
    env.update({
        'WINEPREFIX': str(prefix.resolve()),
        'WINEDEBUG': '-all',
        'DISPLAY': '',
        'WAYLAND_DISPLAY': '',
        'SDL_AUDIODRIVER': 'dummy',
        'ALSOFT_DRIVERS': 'null',
        'WINEDLLOVERRIDES': (
            'winemenubuilder.exe=d;winedbg.exe=d;mscoree=d;mshtml=d;'
            'winealsa.drv=d;winepulse.drv=d;wineoss.drv=d;winecoreaudio.drv=d;'
            'winejack.drv=d;wineaudioio.drv=d'
        ),
    })
    return env


def run_process(args, env=None, timeout=TIMEOUT):
    try:
        result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, env=env, timeout=timeout,
                                check=False)
    except subprocess.TimeoutExpired as exc:
        raise ProcessFailure(f'command timed out after {timeout}s: {args!r}',
                             exc.stdout, exc.stderr) from exc
    except OSError as exc:
        raise ProcessFailure(f'command could not start: {args!r}: {exc}') from exc
    return result


def compare_metadata(windows, linux):
    mismatches = {}
    for key in METADATA_KEYS:
        if windows.get(key) != linux.get(key):
            mismatches[key] = {'windows': windows.get(key), 'linux': linux.get(key)}
    video_hash_match = windows.get('video_sha256') == linux.get('video_sha256')
    frame_counts_match = (windows.get('source_frame_count') == linux.get('source_frame_count')
                          and windows.get('decoded_frame_count') == linux.get('decoded_frame_count'))
    audio_hash_match = windows.get('audio_sha256') == linux.get('audio_sha256')
    return {
        'video_sha256_match': video_hash_match,
        'windows_video_sha256': windows.get('video_sha256'),
        'linux_video_sha256': linux.get('video_sha256'),
        'frame_counts_match': frame_counts_match,
        'metadata_match': not mismatches,
        'metadata_mismatches': mismatches,
        'audio_sha256_match': audio_hash_match,
        'windows_audio_sha256': windows.get('audio_sha256'),
        'linux_audio_sha256': linux.get('audio_sha256'),
        'audio_hash_policy': 'reported only; no cross-architecture PCM tolerance established',
    }


def scan_case(index, source_record, linux_record, scanner, prefix, output, env):
    input_path = Path(source_record['input']).resolve()
    expected_hash = source_record['sha256'].lower()
    linux_scan = linux_record['file_scan']
    case_dir = output / f'case-{index:05d}'
    case_dir.mkdir()
    result = {'index': index, 'input': str(input_path), 'expected_input_sha256': expected_hash,
              'status': 'FAIL', 'commands': {}}
    before = None
    raw_stderr = None
    try:
        before = digest(input_path)
        result['input_sha256_before'] = before
        if before != expected_hash:
            raise RuntimeError(f'corpus input hash mismatch: expected {expected_hash}, got {before}')
        if linux_record.get('input_sha256_before') != expected_hash or linux_record.get('status') != 'PASS':
            raise RuntimeError('corresponding Linux full-video receipt case is not a passing pinned input')
        with tempfile.TemporaryDirectory(prefix='.scan-', dir=case_dir) as temp_name:
            temp = Path(temp_name)
            output_json = temp / 'windows.json'
            args = ['wine', str(scanner), wine_path(input_path), wine_path(output_json)]
            process = run_process(args, env=env)
            result['commands']['windows_scanner'] = args
            raw_stderr = process.stderr
            if process.returncode:
                raise RuntimeError(f'Windows scanner exit {process.returncode}; '
                                   f'stdout={process.stdout[-1000:]!r}; stderr={process.stderr[-2000:]!r}')
            if not output_json.is_file():
                raise RuntimeError(f'Windows scanner exited successfully without JSON output; stderr={process.stderr[-2000:]!r}')
            windows_scan = json.loads(output_json.read_text())
        comparison = compare_metadata(windows_scan, linux_scan)
        result['windows_scan'] = {key: windows_scan.get(key) for key in
                                  (*METADATA_KEYS, 'video_sha256', 'audio_sha256')}
        result['linux_scan'] = {key: linux_scan.get(key) for key in
                                (*METADATA_KEYS, 'video_sha256', 'audio_sha256')}
        result['comparison'] = comparison
        if not comparison['video_sha256_match']:
            raise RuntimeError('full-file video SHA-256 differs from Linux receipt')
        if not comparison['frame_counts_match']:
            raise RuntimeError('source/decoded frame counts differ from Linux receipt')
        if not comparison['metadata_match']:
            raise RuntimeError(f'metadata differs from Linux receipt: {comparison["metadata_mismatches"]}')
        after = digest(input_path)
        result['input_sha256_after'] = after
        result['input_unchanged'] = before == after == expected_hash
        if not result['input_unchanged']:
            raise RuntimeError(f'input hash changed during Windows scan: before={before}, after={after}')
        result['status'] = 'PASS'
    except Exception as exc:
        result['failure'] = str(exc)
        if isinstance(exc, ProcessFailure):
            raw_stderr = exc.stderr
        if raw_stderr is not None:
            stderr_path = case_dir / 'stderr.raw'
            stderr_path.write_bytes(raw_stderr)
            result['stderr_file'] = str(stderr_path)
        try:
            after = digest(input_path)
            result['input_sha256_after'] = after
            result['input_unchanged'] = before == after == expected_hash if before else after == expected_hash
        except Exception as hash_error:
            result['input_hash_after_error'] = str(hash_error)
    return result


def save_receipt(path, receipt):
    with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=path.parent,
                                     prefix='.receipt-', delete=False) as stream:
        temp = Path(stream.name)
        json.dump(receipt, stream, indent=2)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp, path)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus-json', type=Path, default=Path('build/bink-rust-stage1/corpus.json'))
    parser.add_argument('--linux-receipt', type=Path, default=Path('build/bink-rust-stage2/full-video-v4/receipt.json'))
    parser.add_argument('--scanner', type=Path, default=Path('build/bink-rust-stage2/frozen-decoder-v4/bink_scan.exe'))
    parser.add_argument('--output-dir', type=Path, default=Path('build/bink-rust-stage2/windows-full-v4'))
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args(argv)
    if not 1 <= args.jobs <= MAX_JOBS:
        parser.error(f'--jobs must be between 1 and {MAX_JOBS}')
    corpus, linux_receipt = args.corpus_json.resolve(), args.linux_receipt.resolve()
    scanner, output = args.scanner.resolve(), args.output_dir.resolve()
    if not corpus.is_file() or not linux_receipt.is_file():
        parser.error('corpus manifest and Linux full-video receipt must exist')
    if not scanner.is_file():
        parser.error(f'Windows scanner not found: {scanner}')
    if output.exists():
        parser.error(f'output directory already exists: {output}')
    ff = shutil.which('wineboot')
    wine = shutil.which('wine')
    if not ff or not wine:
        parser.error('wine and wineboot are required')
    corpus_hash, linux_receipt_hash, scanner_hash = digest(corpus), digest(linux_receipt), digest(scanner)
    corpus_data = json.loads(corpus.read_text())
    linux_data = json.loads(linux_receipt.read_text())
    sources = corpus_data.get('unique_inputs')
    linux_results = linux_data.get('results')
    if not isinstance(sources, list) or len(sources) != 191:
        parser.error(f'expected 191 unique corpus inputs, found {len(sources) if isinstance(sources, list) else "invalid"}')
    if not isinstance(linux_results, list) or len(linux_results) != len(sources):
        parser.error('Linux full-video receipt result count does not match corpus')
    for index, (source, linux_case) in enumerate(zip(sources, linux_results)):
        if source.get('input') != linux_case.get('input'):
            parser.error(f'corpus/Linux receipt input ordering mismatch at case {index}')

    output.mkdir(parents=True)
    prefix = output / 'wine-prefix'
    prefix.mkdir()
    env = wine_env(prefix)
    receipt_path = output / 'receipt.json'
    receipt = {
        'scope': 'windows_full_file_video_hash_and_metadata_parity',
        'acceptance': 'exact video hash/frame-count/metadata comparison; audio hash reported without PCM tolerance',
        'corpus_json': str(corpus), 'corpus_sha256': corpus_hash,
        'linux_receipt': str(linux_receipt), 'linux_receipt_sha256': linux_receipt_hash,
        'linux_scanner_sha256': linux_data.get('scanner_sha256'),
        'windows_scanner': str(scanner), 'windows_scanner_sha256': scanner_hash,
        'wine_version': None, 'wine_prefix': str(prefix),
        'wine_environment': {key: env[key] for key in ('WINEDEBUG', 'DISPLAY', 'WAYLAND_DISPLAY',
                                                        'WINEDLLOVERRIDES', 'SDL_AUDIODRIVER', 'ALSOFT_DRIVERS')},
        'jobs': args.jobs, 'timeout_seconds_per_scan': TIMEOUT,
        'total_cases': len(sources), 'completed_cases': 0,
        'passed_cases': 0, 'failed_cases': 0, 'results': [None] * len(sources),
    }
    try:
        receipt['wine_version'] = run_process([wine, '--version'], env=env, timeout=15).stdout.decode(errors='replace').strip()
    except Exception as exc:
        receipt['wine_version'] = f'unavailable: {exc}'
    save_receipt(receipt_path, receipt)
    boot = run_process([ff, '-u'], env=env, timeout=180)
    if boot.returncode or not (prefix / 'system.reg').is_file():
        raise RuntimeError(f'private Wine prefix initialization failed: rc={boot.returncode}; '
                           f'stdout={boot.stdout[-1000:]!r}; stderr={boot.stderr[-2000:]!r}')
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(scan_case, i, source, linux_results[i], scanner, prefix, output, env): i
                   for i, source in enumerate(sources)}
        for future in as_completed(futures):
            index = futures[future]
            try:
                result = future.result()
            except Exception as exc:
                result = {'index': index, 'input': sources[index].get('input'),
                          'status': 'FAIL', 'failure': f'worker error: {exc}'}
            receipt['results'][index] = result
            receipt['completed_cases'] += 1
            receipt['passed_cases'] += result['status'] == 'PASS'
            receipt['failed_cases'] += result['status'] != 'PASS'
            save_receipt(receipt_path, receipt)
            if receipt['completed_cases'] % 10 == 0 or receipt['completed_cases'] == len(sources):
                print(json.dumps({'completed': receipt['completed_cases'], 'total': len(sources),
                                  'passed': receipt['passed_cases'], 'failed': receipt['failed_cases'],
                                  'receipt': str(receipt_path)}), flush=True)
    receipt['windows_scanner_sha256_after'] = digest(scanner)
    receipt['linux_receipt_sha256_after'] = digest(linux_receipt)
    receipt['corpus_sha256_after'] = digest(corpus)
    receipt['pins_unchanged'] = (
        receipt['windows_scanner_sha256_after'] == scanner_hash
        and receipt['linux_receipt_sha256_after'] == linux_receipt_hash
        and receipt['corpus_sha256_after'] == corpus_hash)
    if not receipt['pins_unchanged']:
        receipt['failed_cases'] += 1
    receipt['passed'] = receipt['failed_cases'] == 0
    save_receipt(receipt_path, receipt)
    print(json.dumps({'passed': receipt['passed'], 'cases': len(sources),
                      'passed_cases': receipt['passed_cases'], 'failed_cases': receipt['failed_cases'],
                      'pins_unchanged': receipt['pins_unchanged'], 'receipt': str(receipt_path)}, indent=2))
    return 0 if receipt['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
