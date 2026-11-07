#!/usr/bin/env python3
"""Packet-aware full-file comparison of Rust Bink PCM against FFmpeg PCM."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


TIMEOUT = 600
MAX_JOBS = 4
MAX_S16_DELTA = 1
MAX_SOURCE_BYTES = 1 << 30
SUMMARY_KEYS = ('signature', 'width', 'height', 'fps_num', 'fps_den',
                'source_frame_count', 'decoded_frame_count', 'audio_tracks_in_header',
                'audio_sample_rate', 'audio_channels', 'audio_sample_frames',
                'audio_bytes', 'audio_sha256')


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def run_capture(args, timeout=TIMEOUT):
    try:
        result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f'command failed or timed out after {timeout}s: {args!r}: {exc}') from exc
    if result.returncode:
        raise RuntimeError(f'command exit {result.returncode}: {args!r}; stderr={result.stderr[-2000:]!r}')
    return result.stdout


def run_to_file(args, timeout=TIMEOUT):
    try:
        result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f'command failed or timed out after {timeout}s: {args!r}: {exc}') from exc
    if result.returncode:
        raise RuntimeError(f'command exit {result.returncode}: {args!r}; stderr={result.stderr[-2000:]!r}')


def parse_probe_rows(text):
    groups = []
    for row in csv.reader(text.splitlines()):
        if len(row) < 2:
            continue
        try:
            pos, samples = int(row[0]), int(row[1])
        except ValueError as exc:
            raise RuntimeError(f'invalid ffprobe audio frame row: {row!r}') from exc
        if pos < 0 or samples < 0:
            raise RuntimeError(f'negative ffprobe packet position/sample count: {row!r}')
        if groups and groups[-1]['pkt_pos'] == pos:
            groups[-1]['decoded_sample_frames'] += samples
        else:
            groups.append({'pkt_pos': pos, 'decoded_sample_frames': samples})
    return groups


def packet_sample_limits(input_path, channels):
    size = input_path.stat().st_size
    if not 44 <= size <= MAX_SOURCE_BYTES:
        raise RuntimeError(f'source size outside packet parser bound: {size}')
    with input_path.open('rb') as source:
        header = source.read(44)
        if len(header) != 44:
            raise RuntimeError('short Bink header')
        frame_count = struct.unpack_from('<I', header, 8)[0]
        tracks = struct.unpack_from('<I', header, 40)[0]
        if frame_count == 0 or frame_count > 100_000 or tracks != 1:
            raise RuntimeError(f'packet parser requires one track and bounded frames; frames={frame_count}, tracks={tracks}')
        index_start = 44 + 12 * tracks
        index_end = index_start + 4 * (frame_count + 1)
        if index_end > size:
            raise RuntimeError('Bink frame index exceeds source size')
        source.seek(index_start)
        offsets_raw = source.read(4 * (frame_count + 1))
        if len(offsets_raw) != 4 * (frame_count + 1):
            raise RuntimeError('short Bink frame index')
        offsets = struct.unpack('<' + 'I' * (frame_count + 1), offsets_raw)
        limits = {}
        per_frame = [0] * frame_count
        min_pos = index_end
        for frame_index in range(frame_count):
            pos = offsets[frame_index] & ~1
            next_pos = offsets[frame_index + 1] & ~1
            if pos < min_pos or next_pos <= pos or next_pos > size:
                raise RuntimeError(f'invalid frame packet extent at frame {frame_index}')
            packet_size = next_pos - pos
            source.seek(pos)
            raw_len = source.read(4)
            if len(raw_len) != 4:
                raise RuntimeError(f'short audio packet prefix at frame {frame_index}')
            audio_len = struct.unpack('<I', raw_len)[0]
            if audio_len == 0:
                continue
            if audio_len < 4 or audio_len > packet_size - 4:
                raise RuntimeError(f'audio packet length outside frame extent at frame {frame_index}')
            raw_reported = source.read(4)
            if len(raw_reported) != 4:
                raise RuntimeError(f'missing decoded PCM length at frame {frame_index}')
            reported_bytes = struct.unpack('<I', raw_reported)[0]
            divisor = 2 * channels
            if reported_bytes % divisor:
                raise RuntimeError(f'decoded PCM byte count not aligned to s16 channel frames at frame {frame_index}')
            sample_frames = reported_bytes // divisor
            packet_pos = pos + 4
            if packet_pos in limits:
                raise RuntimeError(f'duplicate Bink audio packet position {packet_pos}')
            limits[packet_pos] = {'frame': frame_index, 'sample_frames': sample_frames}
            per_frame[frame_index] = sample_frames
        return frame_count, limits, per_frame


def trim_packet_pcm(raw_pcm, trimmed_pcm, groups, limits, channels):
    raw_bytes = raw_pcm.stat().st_size
    frame_bytes = channels * 2
    if raw_bytes % frame_bytes:
        raise RuntimeError(f'ffmpeg raw PCM length {raw_bytes} is not aligned to {channels}-channel s16 frames')
    raw_sample_frames = raw_bytes // frame_bytes
    cursor = 0
    kept_frames = 0
    removed_frames = 0
    seen_positions = set()
    with raw_pcm.open('rb') as source, trimmed_pcm.open('wb') as target:
        for group in groups:
            pos = group['pkt_pos']
            decoded_frames = group['decoded_sample_frames']
            if pos in seen_positions:
                raise RuntimeError(f'non-contiguous duplicate ffprobe packet position {pos}')
            seen_positions.add(pos)
            packet = limits.get(pos)
            if packet is None:
                raise RuntimeError(f'ffprobe packet position {pos} did not map to a Bink audio packet')
            reported_frames = packet['sample_frames']
            if reported_frames > decoded_frames:
                raise RuntimeError(f'Bink packet {packet["frame"]} reports {reported_frames} PCM frames, '
                                   f'ffmpeg decoded only {decoded_frames}')
            source.seek(cursor * frame_bytes)
            payload = source.read(reported_frames * frame_bytes)
            if len(payload) != reported_frames * frame_bytes:
                raise RuntimeError('short ffmpeg PCM packet segment')
            target.write(payload)
            cursor += decoded_frames
            kept_frames += reported_frames
            removed_frames += decoded_frames - reported_frames
        missing = [pos for pos, packet in limits.items()
                   if packet['sample_frames'] and pos not in seen_positions]
        if missing:
            raise RuntimeError(f'{len(missing)} Bink audio packet(s) missing from ffprobe frame list')
        if cursor != raw_sample_frames:
            raise RuntimeError(f'ffprobe accounted for {cursor} of {raw_sample_frames} raw ffmpeg sample frames')
        target.flush()
        os.fsync(target.fileno())
    return {'ffmpeg_raw_sample_frames': raw_sample_frames,
            'ffmpeg_trimmed_sample_frames': kept_frames,
            'ffmpeg_padding_sample_frames_removed': removed_frames,
            'packet_groups': len(groups)}


def pcm_metrics(candidate_pcm, reference_pcm):
    candidate_bytes = candidate_pcm.stat().st_size
    reference_bytes = reference_pcm.stat().st_size
    if candidate_bytes % 2 or reference_bytes % 2:
        raise RuntimeError('PCM s16le output has odd byte count')
    candidate_samples, reference_samples = candidate_bytes // 2, reference_bytes // 2
    differing = max_delta = sum_sq = compared = 0
    with candidate_pcm.open('rb') as cand, reference_pcm.open('rb') as ref:
        remaining = min(candidate_samples, reference_samples)
        while remaining:
            count = min(8192, remaining)
            cb, rb = cand.read(count * 2), ref.read(count * 2)
            if len(cb) != count * 2 or len(rb) != count * 2:
                raise RuntimeError('PCM data ended before its reported file length')
            cs = struct.unpack('<' + 'h' * count, cb)
            rs = struct.unpack('<' + 'h' * count, rb)
            for a, b in zip(cs, rs):
                delta = abs(a - b)
                differing += delta != 0
                max_delta = max(max_delta, delta)
                sum_sq += delta * delta
            compared += count
            remaining -= count
    rms = math.sqrt(sum_sq / compared) if compared else 0.0
    return {'candidate_pcm_bytes': candidate_bytes,
            'ffmpeg_trimmed_pcm_bytes': reference_bytes,
            'candidate_s16_samples': candidate_samples,
            'ffmpeg_trimmed_s16_samples': reference_samples,
            'sample_count_match': candidate_samples == reference_samples,
            'compared_samples': compared,
            'differing_samples': differing,
            'max_abs_difference_s16': max_delta,
            'rms_difference_s16': rms}


def probe_stream(ffprobe, input_path):
    args = [str(ffprobe), '-v', 'error', '-select_streams', 'a:0', '-show_entries',
            'stream=sample_rate,channels', '-of', 'json', str(input_path)]
    data = json.loads(run_capture(args).decode('utf-8'))
    streams = data.get('streams', [])
    if len(streams) != 1:
        raise RuntimeError(f'ffprobe expected one audio stream, found {len(streams)}')
    try:
        rate, channels = int(streams[0]['sample_rate']), int(streams[0]['channels'])
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError(f'ffprobe audio stream missing rate/channels: {streams[0]!r}') from exc
    if rate <= 0 or channels <= 0:
        raise RuntimeError(f'invalid ffprobe audio format: {rate} Hz, {channels} channels')
    return args, rate, channels


def run_case(index, item, scanner, ffmpeg, ffprobe, output):
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
        tracks = struct.unpack('<I', input_path.open('rb').read()[40:44])[0]
        if tracks != 1:
            raise RuntimeError(f'expected one audio track, found {tracks}')
        with tempfile.TemporaryDirectory(prefix='.audio-', dir=case_dir) as td:
            temp = Path(td)
            candidate_json, candidate_pcm = temp / 'candidate.json', temp / 'candidate.s16le'
            scanner_cmd = [str(scanner), str(input_path), str(candidate_json),
                           '--audio-output', str(candidate_pcm)]
            run_capture(scanner_cmd)
            candidate = json.loads(candidate_json.read_text())
            if not candidate_pcm.is_file():
                raise RuntimeError('scanner did not create candidate PCM output')
            result['commands']['scanner'] = scanner_cmd
            ffprobe_stream_cmd, ff_rate, ff_channels = probe_stream(ffprobe, input_path)
            ffprobe_frames_cmd = [str(ffprobe), '-v', 'error', '-select_streams', 'a:0',
                                  '-show_frames', '-show_entries', 'frame=pkt_pos,nb_samples',
                                  '-of', 'csv=p=0', str(input_path)]
            rows_text = run_capture(ffprobe_frames_cmd).decode('utf-8', errors='replace')
            groups = parse_probe_rows(rows_text)
            frame_count, packet_limits, declared_per_frame = packet_sample_limits(input_path, ff_channels)
            ffmpeg_raw = temp / 'ffmpeg.s16le'
            ffmpeg_cmd = [str(ffmpeg), '-nostdin', '-v', 'error', '-threads', '1', '-i',
                          str(input_path), '-map', '0:a:0', '-vn', '-c:a', 'pcm_s16le',
                          '-f', 's16le', str(ffmpeg_raw)]
            run_to_file(ffmpeg_cmd)
            ffmpeg_trimmed = temp / 'ffmpeg-trimmed.s16le'
            trim = trim_packet_pcm(ffmpeg_raw, ffmpeg_trimmed, groups, packet_limits, ff_channels)
            metrics = pcm_metrics(candidate_pcm, ffmpeg_trimmed)
            failures = []
            if candidate.get('audio_tracks_in_header') != 1:
                failures.append('scanner did not report exactly one audio track')
            if candidate.get('audio_sample_rate') != ff_rate or candidate.get('audio_channels') != ff_channels:
                failures.append('scanner and ffprobe audio rate/channel metadata differ')
            if candidate.get('decoded_frame_count') != frame_count:
                failures.append('scanner and source frame counts differ')
            if candidate.get('per_frame_audio_sample_frames') != declared_per_frame:
                mismatches = [{'frame': i, 'candidate': candidate['per_frame_audio_sample_frames'][i],
                               'packet_prefix': declared_per_frame[i]}
                              for i in range(min(len(candidate.get('per_frame_audio_sample_frames', [])), len(declared_per_frame)))
                              if candidate['per_frame_audio_sample_frames'][i] != declared_per_frame[i]]
                failures.append(f'candidate and packet-prefix sample counts differ at {len(mismatches)} frame(s)')
                result['per_frame_count_mismatches'] = mismatches[:20]
            expected_sample_frames = sum(declared_per_frame)
            if candidate.get('audio_sample_frames') != expected_sample_frames:
                failures.append('candidate total sample-frame metadata differs from packet prefixes')
            if not metrics['sample_count_match']:
                failures.append(f'full PCM sample count mismatch: candidate={metrics["candidate_s16_samples"]}, '
                                f'ffmpeg_trimmed={metrics["ffmpeg_trimmed_s16_samples"]}')
            if metrics['candidate_s16_samples'] != expected_sample_frames * ff_channels:
                failures.append('candidate PCM length differs from packet-prefix sample counts')
            if metrics['max_abs_difference_s16'] > MAX_S16_DELTA:
                failures.append(f'max s16 PCM difference {metrics["max_abs_difference_s16"]} exceeds fixed criterion {MAX_S16_DELTA}')
            result['commands'].update(ffprobe_stream=ffprobe_stream_cmd,
                                      ffprobe_frames=ffprobe_frames_cmd,
                                      ffmpeg_pcm=ffmpeg_cmd)
            result['candidate'] = {key: candidate.get(key) for key in SUMMARY_KEYS}
            result['ffprobe'] = {'sample_rate': ff_rate, 'channels': ff_channels}
            result['packet_trim'] = trim
            result['pcm'] = metrics
            result['criterion'] = {'max_abs_difference_s16_at_most': MAX_S16_DELTA,
                                  'meaning': 'explicit candidate parity criterion, not a general audio-quality claim'}
            if failures:
                raise RuntimeError('; '.join(failures))
        after = digest(input_path)
        result['input_sha256_after'] = after
        result['input_unchanged'] = before == after == expected_hash
        if not result['input_unchanged']:
            raise RuntimeError(f'input changed during comparison: before={before}, after={after}')
        result['status'] = 'PASS'
    except Exception as exc:
        result['failure'] = str(exc)
        try:
            after = digest(input_path)
            result['input_sha256_after'] = after
            result['input_unchanged'] = before == after == expected_hash if before else after == expected_hash
        except Exception as hash_exc:
            result['input_hash_after_error'] = str(hash_exc)
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
    parser.add_argument('--scanner', type=Path, default=Path('build/bink-rust-stage2/frozen-decoder-v4/bink_scan'))
    parser.add_argument('--output-dir', type=Path, default=Path('build/bink-rust-stage2/ffmpeg-audio-full-v4'))
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    args = parser.parse_args(argv)
    if not 1 <= args.jobs <= MAX_JOBS:
        parser.error(f'--jobs must be between 1 and {MAX_JOBS}')
    corpus, scanner, output = args.corpus_json.resolve(), args.scanner.resolve(), args.output_dir.resolve()
    if not corpus.is_file():
        parser.error(f'corpus manifest not found: {corpus}')
    if not scanner.is_file() or not os.access(scanner, os.X_OK):
        parser.error(f'frozen scanner must be executable: {scanner}')
    if output.exists():
        parser.error(f'output directory already exists: {output}')
    ffmpeg, ffprobe = shutil.which(args.ffmpeg), shutil.which(args.ffprobe)
    if not ffmpeg or not ffprobe:
        parser.error('ffmpeg and ffprobe are required')
    data = json.loads(corpus.read_text())
    all_inputs = data.get('unique_inputs')
    if not isinstance(all_inputs, list):
        parser.error('corpus manifest must contain unique_inputs')
    inputs = []
    for item in all_inputs:
        path = Path(item['input'])
        with path.open('rb') as source:
            source.seek(40)
            tracks = struct.unpack('<I', source.read(4))[0]
        if tracks:
            inputs.append(item)
    if len(inputs) != 114:
        parser.error(f'expected the frozen corpus audio matrix of 114 assets, found {len(inputs)}')
    output.mkdir(parents=True)
    receipt_path = output / 'receipt.json'
    receipt = {'scope': 'packet-aware_full-file_audio_pcm_parity',
               'acceptance': 'candidate s16le vs packet-trimmed ffmpeg s16le only; no native/game/audio-device claim',
               'corpus_json': str(corpus), 'corpus_sha256': digest(corpus),
               'scanner': str(scanner), 'scanner_sha256': digest(scanner),
               'ffmpeg': ffmpeg, 'ffprobe': ffprobe, 'jobs': args.jobs,
               'timeout_seconds_per_process': TIMEOUT,
               'max_s16_delta': MAX_S16_DELTA,
               'max_s16_delta_claim': 'fixed candidate parity criterion, not a general audio-quality claim',
               'total_cases': len(inputs), 'completed_cases': 0,
               'passed_cases': 0, 'failed_cases': 0, 'results': [None] * len(inputs)}
    for key, cmd in (('scanner_sha256_at_start', None),
                     ('ffmpeg_version', [ffmpeg, '-version']),
                     ('ffprobe_version', [ffprobe, '-version'])):
        if cmd:
            try:
                receipt[key] = run_capture(cmd, timeout=10).decode(errors='replace').splitlines()[0]
            except Exception as exc:
                receipt[key] = f'unavailable: {exc}'
        else:
            receipt[key] = digest(scanner)
    save_receipt(receipt_path, receipt)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(run_case, i, item, scanner, ffmpeg, ffprobe, output): i
                   for i, item in enumerate(inputs)}
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
                print(json.dumps({'completed': receipt['completed_cases'], 'total': len(inputs),
                                  'passed': receipt['passed_cases'], 'failed': receipt['failed_cases'],
                                  'receipt': str(receipt_path)}), flush=True)
    receipt['passed'] = receipt['failed_cases'] == 0
    save_receipt(receipt_path, receipt)
    print(json.dumps({'passed': receipt['passed'], 'cases': len(inputs),
                      'passed_cases': receipt['passed_cases'], 'failed_cases': receipt['failed_cases'],
                      'receipt': str(receipt_path)}, indent=2))
    return 0 if receipt['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
