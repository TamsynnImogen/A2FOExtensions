#!/usr/bin/env python3
"""Compare candidate Bink audio output with a pinned native DLL track probe."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


PINNED_DLL_SHA256 = '9c2b75defd3bf13737fdb6e8b7cd3b67d7b2d6fe0bbf48ec6dc2ecf1b03e559b'
TIMEOUT = 300
MAX_SOURCE_BYTES = 1 << 30
MAX_FRAMES = 100_000
MAX_AUDIO_DELTA = 1


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def command(args, env=None, timeout=TIMEOUT):
    try:
        result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, env=env, timeout=timeout,
                                check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f'command failed or timed out after {timeout}s: {args!r}: {exc}') from exc
    if result.returncode:
        raise RuntimeError(f'command exit {result.returncode}: {args!r}; '
                           f'stdout={result.stdout[-1000:]!r}; stderr={result.stderr[-2000:]!r}')
    return result.stdout


def wine_path(path):
    resolved = Path(path).resolve()
    return 'Z:' + str(resolved).replace('/', '\\')


def run_native(probe, dll, input_path, output_dir, env):
    args = ['wine', str(probe), wine_path(dll), wine_path(input_path),
            wine_path(output_dir), '0', '0xffffffff']
    command(args, env=env)
    meta_path = output_dir / 'metadata.json'
    if not meta_path.is_file():
        raise RuntimeError('native probe did not produce metadata.json')
    return args, json.loads(meta_path.read_text())


def run_scanner(scanner, input_path, output_dir):
    metadata = output_dir / 'candidate.json'
    pcm = output_dir / 'candidate.pcm'
    args = [str(scanner), str(input_path), str(metadata), '--audio-output', str(pcm)]
    command(args)
    if not metadata.is_file() or not pcm.is_file():
        raise RuntimeError('scanner did not produce candidate JSON and PCM outputs')
    return args, json.loads(metadata.read_text()), pcm


def compare_pcm(candidate_pcm, native_pcm, bits):
    if bits not in (8, 16):
        raise RuntimeError(f'unsupported native audio bit depth: {bits}')
    candidate_bytes = candidate_pcm.stat().st_size
    native_bytes = native_pcm.stat().st_size
    if candidate_bytes % 2:
        raise RuntimeError(f'candidate signed-16 PCM has an odd byte count: {candidate_bytes}')
    if bits == 16 and native_bytes % 2:
        raise RuntimeError(f'native signed-16 PCM has an odd byte count: {native_bytes}')
    candidate_samples = candidate_bytes // 2
    native_samples = native_bytes // (bits // 8)
    overlap = min(candidate_samples, native_samples)
    max_delta = 0
    differing = 0
    sum_sq = 0
    raw16_max_delta = 0
    raw16_differing = 0
    raw16_sum_sq = 0
    compared = 0
    with candidate_pcm.open('rb') as cand, native_pcm.open('rb') as native:
        remaining = overlap
        while remaining:
            count = min(8192, remaining)
            raw_c = cand.read(count * 2)
            raw_n = native.read(count * (bits // 8))
            if len(raw_c) != count * 2 or len(raw_n) != count * (bits // 8):
                raise RuntimeError('PCM file ended before its reported size')
            samples_c = struct.unpack('<' + 'h' * count, raw_c)
            if bits == 16:
                samples_n = struct.unpack('<' + 'h' * count, raw_n)
                quantized_pairs = zip(samples_c, samples_n)
            else:
                samples_n = tuple((value - 128) << 8 for value in raw_n)
                quantized_pairs = ((max(0, min(255, (sample >> 8) + 128)), native)
                                   for sample, native in zip(samples_c, raw_n))
            for a, b in quantized_pairs:
                delta = abs(a - b)
                differing += delta != 0
                max_delta = max(max_delta, delta)
                sum_sq += delta * delta
            if bits == 8:
                for a, b in zip(samples_c, samples_n):
                    raw_delta = abs(a - b)
                    raw16_differing += raw_delta != 0
                    raw16_max_delta = max(raw16_max_delta, raw_delta)
                    raw16_sum_sq += raw_delta * raw_delta
            compared += count
            remaining -= count
    return {
        'candidate_pcm_bytes': candidate_bytes,
        'native_pcm_bytes': native_bytes,
        'candidate_samples': candidate_samples,
        'native_samples': native_samples,
        'compared_samples': compared,
        'sample_count_match': candidate_samples == native_samples,
        'differing_samples': differing,
        'max_abs_difference': max_delta,
        'rms_difference': math.sqrt(sum_sq / compared) if compared else 0.0,
        'comparison_units': 'native signed 16-bit samples' if bits == 16 else 'native unsigned 8-bit codes',
        'native_8bit_quantization': 'candidate: clip((signed16 >> 8) + 128, 0, 255); compare unsigned codes' if bits == 8 else None,
        'raw16_normalized_diagnostic': ({
            'meaning': 'diagnostic only: native unsigned 8-bit normalized by (value - 128) * 256, compared in signed-16 units',
            'differing_samples': raw16_differing,
            'max_abs_difference': raw16_max_delta,
            'rms_difference': math.sqrt(raw16_sum_sq / compared) if compared else 0.0,
        } if bits == 8 else None),
    }


def compare_results(candidate, candidate_pcm, native, native_pcm):
    track = native.get('track_prefix')
    if not isinstance(track, list) or len(track) < 4:
        raise RuntimeError('native probe metadata has no valid track_prefix')
    native_rate, native_bits, native_channels, native_max_bytes = track[:4]
    if candidate.get('audio_tracks_in_header') != 1:
        raise RuntimeError('candidate did not report exactly one supported audio track')
    candidate_rate = candidate.get('audio_sample_rate')
    candidate_channels = candidate.get('audio_channels')
    failures = []
    if candidate_rate != native_rate or candidate_channels != native_channels:
        failures.append(f'audio format mismatch: candidate rate/channels={candidate_rate}/{candidate_channels}, '
                        f'native={native_rate}/{native_channels}')
    open_prefix = native.get('open_prefix')
    if not isinstance(open_prefix, list) or len(open_prefix) < 3:
        raise RuntimeError('native probe metadata has no valid open_prefix')
    native_frames = native.get('frames')
    candidate_frames = candidate.get('per_frame_audio_sample_frames')
    expected_frames = open_prefix[2]
    if candidate.get('decoded_frame_count') != expected_frames or len(native_frames or []) != expected_frames:
        failures.append('native/candidate decoded frame counts differ')
    if not isinstance(candidate_frames, list) or len(candidate_frames) != expected_frames:
        raise RuntimeError('candidate per-frame audio sample counts are missing or incomplete')
    native_per_frame = []
    sample_bytes = native_bits // 8 if native_bits in (8, 16) else 0
    if not sample_bytes or native_channels <= 0:
        raise RuntimeError(f'unsupported native track format: bits={native_bits}, channels={native_channels}')
    for frame in native_frames:
        audio_bytes = frame.get('audio_bytes')
        if not isinstance(audio_bytes, int) or audio_bytes < 0 or audio_bytes % (sample_bytes * native_channels):
            raise RuntimeError('native per-frame audio byte count is invalid for its track format')
        native_per_frame.append(audio_bytes // (sample_bytes * native_channels))
    frame_mismatches = [{'frame': i, 'candidate_sample_frames': candidate_frames[i],
                         'native_sample_frames': native_per_frame[i]}
                        for i in range(min(len(native_per_frame), len(candidate_frames)))
                        if native_per_frame[i] != candidate_frames[i]]
    if frame_mismatches:
        failures.append(f'per-frame audio sample count mismatch at {len(frame_mismatches)} frame(s)')
    if len(native_per_frame) != len(candidate_frames):
        failures.append('native/candidate per-frame sample count array lengths differ')
    pcm = compare_pcm(candidate_pcm, native_pcm, native_bits)
    expected_candidate_bytes = candidate.get('audio_bytes')
    if expected_candidate_bytes != pcm['candidate_pcm_bytes']:
        failures.append('candidate audio PCM file length does not match scanner metadata')
    native_total = sum(frame['audio_bytes'] for frame in native_frames)
    if native_total != pcm['native_pcm_bytes']:
        failures.append('native audio PCM file length does not match per-frame metadata')
    if not pcm['sample_count_match']:
        failures.append(f'audio sample count mismatch: candidate={pcm["candidate_samples"]}, native={pcm["native_samples"]}')
    if pcm['max_abs_difference'] > MAX_AUDIO_DELTA:
        failures.append(f'audio max absolute delta {pcm["max_abs_difference"]} {pcm["comparison_units"]} '
                        f'exceeds fixed criterion {MAX_AUDIO_DELTA}')
    return {'passed': not failures, 'failures': failures,
            'candidate_audio_sample_rate': candidate_rate,
            'native_audio_sample_rate': native_rate,
            'candidate_audio_channels': candidate_channels,
            'native_audio_channels': native_channels,
            'native_audio_bits': native_bits,
            'native_audio_max_bytes': native_max_bytes,
            'per_frame_sample_count_mismatches': frame_mismatches,
            'criterion': {'max_abs_difference_at_most': MAX_AUDIO_DELTA,
                         'units': pcm['comparison_units'],
                         'meaning': 'explicit candidate parity criterion; not a general audio-quality claim'},
            'pcm': pcm}


def native_environment(prefix):
    env = os.environ.copy()
    env.update({'WINEPREFIX': str(Path(prefix).resolve()), 'WINEDEBUG': '-all',
                'DISPLAY': '', 'WAYLAND_DISPLAY': '',
                'WINEDLLOVERRIDES': 'winemenubuilder.exe=d;mscoree=d;mshtml=d;winedbg.exe=d'})
    return env


def scan_track_count(path):
    with path.open('rb') as stream:
        stream.seek(40)
        raw = stream.read(4)
    if len(raw) != 4:
        raise RuntimeError('input too short to contain Bink audio track count')
    return int.from_bytes(raw, 'little')


def run_case(index, item, scanner, probe, dll, prefix, output, expected_dll_hash):
    input_path = Path(item['input']).resolve()
    expected_hash = item['sha256'].lower()
    case_dir = output / f'case-{index:05d}'
    case_dir.mkdir()
    result = {'index': index, 'input': str(input_path), 'expected_input_sha256': expected_hash,
              'status': 'FAIL', 'commands': {}}
    before = None
    try:
        before = digest(input_path)
        result['input_sha256_before'] = before
        if before != expected_hash:
            raise RuntimeError(f'corpus source hash mismatch: expected {expected_hash}, got {before}')
        if digest(dll) != expected_dll_hash:
            raise RuntimeError('pinned native DLL changed during matrix run')
        if scan_track_count(input_path) == 0:
            result['status'] = 'SKIP'
            result['skip_reason'] = 'no audio tracks in Bink header'
        else:
            native_dir = case_dir / 'native'
            scanner_cmd, candidate, candidate_pcm = run_scanner(scanner, input_path, case_dir)
            result['commands']['scanner'] = scanner_cmd
            result['candidate'] = {k: candidate.get(k) for k in (
                'signature', 'source_frame_count', 'decoded_frame_count', 'audio_tracks_in_header',
                'audio_sample_rate', 'audio_channels', 'audio_sample_frames', 'audio_bytes', 'audio_sha256')}
            native_env = native_environment(prefix)
            native_cmd, native = run_native(probe, dll, input_path, native_dir, native_env)
            result['commands']['native_probe'] = native_cmd
            native_pcm = native_dir / 'audio.bin'
            if not native_pcm.is_file():
                raise RuntimeError('native probe did not produce audio.bin for audio-bearing input')
            result['native'] = {'sound_factory_result': native.get('sound_factory_result'),
                                'copy_flags': native.get('copy_flags'),
                                'open_prefix': native.get('open_prefix'),
                                'track_prefix': native.get('track_prefix'),
                                'native_frame_count': len(native.get('frames', []))}
            result['comparison'] = compare_results(candidate, candidate_pcm, native, native_pcm)
            if not result['comparison']['passed']:
                raise RuntimeError('; '.join(result['comparison']['failures']))
            result['status'] = 'PASS'
        after = digest(input_path)
        result['input_sha256_after'] = after
        result['input_unchanged'] = before == after == expected_hash
        if not result['input_unchanged']:
            raise RuntimeError(f'input changed during comparison: before={before}, after={after}')
    except Exception as exc:
        result['failure'] = str(exc)
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
    parser.add_argument('--corpus-json', type=Path, required=True)
    parser.add_argument('--scanner', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--native-dll', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args(argv)
    corpus = args.corpus_json.resolve()
    scanner = args.scanner.resolve()
    probe = args.probe.resolve()
    dll = args.native_dll.resolve()
    prefix = args.wine_prefix.resolve()
    output = args.output_dir.resolve()
    for path, label in ((corpus, 'corpus manifest'), (scanner, 'scanner'),
                        (probe, 'native probe'), (dll, 'native DLL')):
        if not path.is_file():
            parser.error(f'{label} not found: {path}')
    if not os.access(scanner, os.X_OK) or not os.access(probe, os.X_OK):
        parser.error('scanner and native probe must be executable')
    if not (prefix / 'system.reg').is_file():
        parser.error(f'initialized Wine prefix required: {prefix}')
    if output.exists():
        parser.error(f'output directory already exists: {output}')
    dll_hash = digest(dll)
    if dll_hash != PINNED_DLL_SHA256:
        parser.error(f'native DLL SHA-256 mismatch: expected {PINNED_DLL_SHA256}, got {dll_hash}')
    data = json.loads(corpus.read_text())
    inputs = data.get('unique_inputs')
    if not isinstance(inputs, list) or not inputs:
        parser.error('corpus manifest must contain unique_inputs')
    output.mkdir(parents=True)
    receipt_path = output / 'receipt.json'
    receipt = {'scope': 'native_audio_pcm_parity_only',
               'acceptance': 'audio PCM and metadata parity; no video or playback acceptance',
               'corpus_json': str(corpus), 'corpus_sha256': digest(corpus),
               'scanner': str(scanner), 'scanner_sha256': digest(scanner),
               'native_probe': str(probe), 'native_probe_sha256': digest(probe),
               'native_dll': str(dll), 'native_dll_sha256': dll_hash,
               'wine_prefix': str(prefix), 'timeout_seconds_per_process': TIMEOUT,
               'max_audio_delta': MAX_AUDIO_DELTA,
               'max_audio_delta_claim': 'fixed candidate criterion only, not a general quality claim',
               'total_cases': len(inputs), 'completed_cases': 0,
               'passed_cases': 0, 'failed_cases': 0, 'skipped_cases': 0,
               'results': [None] * len(inputs)}
    for name, argsv in (('scanner_version', [str(scanner), '--version']),
                        ('wine_version', ['wine', '--version'])):
        try:
            receipt[name] = command(argsv, timeout=10).decode(errors='replace').strip()
        except Exception as exc:
            receipt[name] = f'unavailable: {exc}'
    save_receipt(receipt_path, receipt)
    for index, item in enumerate(inputs):
        if not isinstance(item, dict) or not isinstance(item.get('input'), str) or not isinstance(item.get('sha256'), str):
            result = {'index': index, 'status': 'FAIL', 'failure': 'invalid corpus entry'}
        else:
            result = run_case(index, item, scanner, probe, dll, prefix, output, dll_hash)
        receipt['results'][index] = result
        receipt['completed_cases'] += 1
        if result['status'] == 'PASS':
            receipt['passed_cases'] += 1
        elif result['status'] == 'SKIP':
            receipt['skipped_cases'] += 1
        else:
            receipt['failed_cases'] += 1
        save_receipt(receipt_path, receipt)
        if receipt['completed_cases'] % 10 == 0 or receipt['completed_cases'] == len(inputs):
            print(json.dumps({'completed': receipt['completed_cases'], 'total': len(inputs),
                              'passed': receipt['passed_cases'], 'failed': receipt['failed_cases'],
                              'skipped': receipt['skipped_cases'], 'receipt': str(receipt_path)}),
                  flush=True)
    receipt['passed'] = receipt['failed_cases'] == 0
    save_receipt(receipt_path, receipt)
    print(json.dumps({'passed': receipt['passed'], 'cases': len(inputs),
                      'passed_cases': receipt['passed_cases'], 'failed_cases': receipt['failed_cases'],
                      'skipped_cases': receipt['skipped_cases'], 'receipt': str(receipt_path)}, indent=2))
    return 0 if receipt['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
