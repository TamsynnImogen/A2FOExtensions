#!/usr/bin/env python3
"""Compare full-file video hashes from bink_scan and headless ffmpeg."""
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


TIMEOUT = 600
IGNORED_MEMORY_KEYS = {'elapsed_seconds', 'scan_mode'}
SCAN_SUMMARY_KEYS = (
    'signature', 'width', 'height', 'chroma_width', 'chroma_height',
    'fps_num', 'fps_den', 'source_frame_count', 'decoded_frame_count',
    'has_alpha', 'audio_tracks_in_header', 'audio_sample_rate',
    'audio_channels', 'audio_sample_frames', 'source_bytes', 'video_bytes',
    'audio_bytes', 'alpha_bytes', 'video_sha256', 'audio_sha256',
    'alpha_sha256', 'elapsed_seconds', 'scan_mode')


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def command(command, timeout=TIMEOUT):
    try:
        result = subprocess.run(command, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f'command failed or timed out after {timeout}s: {command!r}: {exc}') from exc
    if result.returncode:
        raise RuntimeError(f'command exit {result.returncode}: {command!r}; '
                           f'stderr={result.stderr[-2000:]!r}')
    return result.stdout


def scan(scanner, input_path, output_path, memory=False):
    args = [str(scanner), str(input_path), str(output_path)]
    if memory:
        args.append('--memory')
    command(args)
    return args, json.loads(output_path.read_text())


def ffmpeg_hash(ffmpeg, input_path):
    args = [str(ffmpeg), '-nostdin', '-v', 'error', '-threads', '1', '-i',
            str(input_path), '-map', '0:v:0', '-an', '-vsync', '0', '-pix_fmt',
            'yuv420p', '-c:v', 'rawvideo', '-f', 'hash', '-hash', 'sha256', '-']
    output = command(args).decode('ascii', errors='replace').strip()
    for line in reversed(output.splitlines()):
        if line.startswith('SHA256='):
            return args, line.partition('=')[2].strip().lower()
    raise RuntimeError(f'ffmpeg hash output missing SHA256 line: {output[-500:]!r}')


def probe(ffprobe, input_path):
    if ffprobe is None:
        return None, None, 'not available'
    args = [str(ffprobe), '-v', 'error', '-show_entries',
            'stream=codec_type,width,height,r_frame_rate,avg_frame_rate,nb_frames,sample_rate,channels',
            '-of', 'json', str(input_path)]
    try:
        return args, json.loads(command(args).decode('utf-8')), None
    except Exception as exc:
        return args, None, str(exc)


def stable_scan_diff(file_scan, memory_scan):
    keys = (set(file_scan) | set(memory_scan)) - IGNORED_MEMORY_KEYS
    return {key: {'file': file_scan.get(key), 'memory': memory_scan.get(key)}
            for key in sorted(keys) if file_scan.get(key) != memory_scan.get(key)}


def summarize_scan(report):
    return {key: report.get(key) for key in SCAN_SUMMARY_KEYS if key in report}


def case_worker(index, item, scanner, ffmpeg, ffprobe, output, use_memory):
    input_path = Path(item['input']).resolve()
    expected = item['sha256'].lower()
    case_dir = output / f'case-{index:05d}'
    case_dir.mkdir()
    result = {'index': index, 'input': str(input_path), 'expected_input_sha256': expected,
              'status': 'FAIL', 'commands': {}}
    before = None
    try:
        before = digest(input_path)
        result['input_sha256_before'] = before
        if before != expected:
            raise RuntimeError(f'corpus input hash mismatch: expected {expected}, got {before}')
        scan_cmd, scan_json = scan(scanner, input_path, case_dir / 'file.json')
        result['commands']['scanner_file'] = scan_cmd
        result['file_scan'] = summarize_scan(scan_json)
        ff_cmd, oracle_hash = ffmpeg_hash(ffmpeg, input_path)
        result['commands']['ffmpeg_video_hash'] = ff_cmd
        result['ffmpeg_video_sha256'] = oracle_hash
        result['video_sha256_match'] = scan_json.get('video_sha256') == oracle_hash
        result['video_bytes'] = scan_json.get('video_bytes')
        probe_cmd, probe_data, probe_error = probe(ffprobe, input_path)
        result['commands']['ffprobe'] = probe_cmd
        result['ffprobe'] = probe_data
        result['ffprobe_error'] = probe_error
        if use_memory:
            mem_cmd, memory_json = scan(scanner, input_path, case_dir / 'memory.json', memory=True)
            result['commands']['scanner_memory'] = mem_cmd
            result['memory_scan'] = summarize_scan(memory_json)
            differences = stable_scan_diff(scan_json, memory_json)
            result['file_memory_differences'] = differences
            result['file_memory_match'] = not differences
        after = digest(input_path)
        result['input_sha256_after'] = after
        result['input_unchanged'] = before == after == expected
        if not result['input_unchanged']:
            raise RuntimeError(f'input hash changed during scan: before={before}, after={after}')
        if not result['video_sha256_match']:
            raise RuntimeError('full-file video SHA-256 mismatch')
        if use_memory and not result['file_memory_match']:
            raise RuntimeError('file and memory scanner outputs differ outside elapsed_seconds/scan_mode')
        result['status'] = 'PASS'
    except Exception as exc:
        result['failure'] = str(exc)
        try:
            after = digest(input_path)
            result['input_sha256_after'] = after
            result['input_unchanged'] = before == after == expected if before else after == expected
        except Exception as hash_error:
            result['input_sha256_after_error'] = str(hash_error)
    return result


def save_receipt(path, receipt):
    with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=path.parent,
                                     prefix='.receipt-', delete=False) as stream:
        temp_path = Path(stream.name)
        json.dump(receipt, stream, indent=2)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp_path, path)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus-json', type=Path,
                        default=Path('build/bink-rust-stage1/corpus.json'))
    parser.add_argument('--scanner', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--memory', action='store_true')
    args = parser.parse_args(argv)
    if not 1 <= args.jobs <= 4:
        parser.error('--jobs must be between 1 and 4')
    scanner = args.scanner.resolve()
    corpus = args.corpus_json.resolve()
    output = args.output_dir.resolve()
    if not scanner.is_file() or not os.access(scanner, os.X_OK):
        parser.error(f'scanner must be an executable file: {scanner}')
    if not corpus.is_file():
        parser.error(f'corpus manifest not found: {corpus}')
    if output.exists():
        parser.error(f'output directory already exists: {output}')
    ffmpeg = shutil.which(args.ffmpeg)
    if ffmpeg is None:
        parser.error(f'ffmpeg not found: {args.ffmpeg}')
    ffprobe = shutil.which(args.ffprobe)
    data = json.loads(corpus.read_text())
    inputs = data.get('unique_inputs')
    if not isinstance(inputs, list) or not inputs:
        parser.error('corpus manifest must contain a non-empty unique_inputs list')
    for item in inputs:
        if not isinstance(item, dict) or not isinstance(item.get('input'), str) or not isinstance(item.get('sha256'), str):
            parser.error('each unique_inputs entry must contain input and sha256 strings')
    output.mkdir(parents=True)
    receipt_path = output / 'receipt.json'
    receipt = {'scope': 'full_file_video_hash_only',
               'acceptance': 'video full-file SHA-256 comparison only; audio is not accepted',
               'corpus_json': str(corpus), 'corpus_sha256': digest(corpus),
               'scanner': str(scanner), 'scanner_sha256': digest(scanner),
               'ffmpeg': ffmpeg, 'ffprobe': ffprobe,
               'jobs': args.jobs, 'timeout_seconds_per_command': TIMEOUT,
               'memory_mode_requested': args.memory, 'total_cases': len(inputs),
               'completed_cases': 0, 'passed_cases': 0, 'failed_cases': 0,
               'results': [None] * len(inputs)}
    try:
        receipt['scanner_version'] = command([str(scanner), '--version'], timeout=10).decode(errors='replace').strip()
    except Exception as exc:
        receipt['scanner_version'] = f'unavailable: {exc}'
    try:
        receipt['ffmpeg_version'] = command([ffmpeg, '-version'], timeout=10).decode(errors='replace').splitlines()[0]
    except Exception as exc:
        receipt['ffmpeg_version'] = f'unavailable: {exc}'
    if ffprobe:
        try:
            receipt['ffprobe_version'] = command([ffprobe, '-version'], timeout=10).decode(errors='replace').splitlines()[0]
        except Exception as exc:
            receipt['ffprobe_version'] = f'unavailable: {exc}'
    save_receipt(receipt_path, receipt)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(case_worker, i, item, scanner, ffmpeg, ffprobe,
                               output, args.memory): i for i, item in enumerate(inputs)}
        for future in as_completed(futures):
            index = futures[future]
            try:
                result = future.result()
            except Exception as exc:
                result = {'index': index, 'input': inputs[index].get('input'),
                          'status': 'FAIL', 'failure': f'worker error: {exc}'}
            receipt['results'][index] = result
            receipt['completed_cases'] += 1
            receipt['passed_cases'] += result['status'] == 'PASS'
            receipt['failed_cases'] += result['status'] != 'PASS'
            save_receipt(receipt_path, receipt)
            if receipt['completed_cases'] % 10 == 0 or receipt['completed_cases'] == len(inputs):
                print(json.dumps({'completed': receipt['completed_cases'],
                                  'total': len(inputs), 'passed': receipt['passed_cases'],
                                  'failed': receipt['failed_cases'],
                                  'receipt': str(receipt_path)}), flush=True)
    receipt['passed'] = receipt['failed_cases'] == 0
    save_receipt(receipt_path, receipt)
    print(json.dumps({'passed': receipt['passed'], 'cases': len(inputs),
                      'passed_cases': receipt['passed_cases'],
                      'failed_cases': receipt['failed_cases'],
                      'receipt': str(receipt_path)}, indent=2))
    return 0 if receipt['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
