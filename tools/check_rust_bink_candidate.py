#!/usr/bin/env python3
"""Headless decoded-frame comparison of a Rust Bink candidate against ffmpeg."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import math
import array
import shutil
import subprocess
import sys
import tempfile


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def run(command, timeout=180):
    try:
        result = subprocess.run(command, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f'command failed or timed out: {command!r}: {exc}') from exc
    if result.returncode:
        raise RuntimeError(f'command exit {result.returncode}: {command!r}; '
                           f'stderr={result.stderr[-2000:]!r}')
    return result.stdout


def compare(case, candidate, ffmpeg, input_path, args):
    target = case / 'candidate'
    command = [str(candidate), 'decode', str(input_path), '--output-dir', str(target),
               '--max-frames', str(args.max_frames), '--start-frame', str(args.start_frame)]
    result = {'input': str(input_path), 'input_sha256': sha(input_path),
              'candidate_command': command}
    try:
        run(command, timeout=300)
        meta_path = target / 'metadata.json'
        meta = json.loads(meta_path.read_text())
        required = ('signature', 'width', 'height', 'fps_num', 'fps_den',
                    'source_frame_count', 'decoded_frame_count', 'pixel_format',
                    'chroma_width', 'chroma_height', 'has_alpha',
                    'audio_tracks_in_header', 'audio_sample_frames', 'video_bytes',
                    'alpha_bytes', 'audio_bytes', 'start_frame')
        missing = [key for key in required if key not in meta]
        if missing:
            raise RuntimeError(f'metadata missing keys: {missing}')
        if meta['pixel_format'] != 'yuv420p' or meta['width'] <= 0 or meta['height'] <= 0:
            raise RuntimeError('unsupported or invalid candidate video geometry/format')
        count, width, height = meta['decoded_frame_count'], meta['width'], meta['height']
        if not 0 < count <= args.max_frames:
            raise RuntimeError(f'invalid decoded_frame_count: {count}')
        yuv = target / 'video.yuv'
        expected_size = width * height * count + 2 * meta['chroma_width'] * meta['chroma_height'] * count
        if yuv.stat().st_size != expected_size or meta['video_bytes'] != expected_size:
            raise RuntimeError('candidate video byte count does not match metadata geometry')
        if meta['start_frame'] != args.start_frame:
            raise RuntimeError('candidate metadata start_frame mismatch')
        audio_bytes = meta['audio_bytes']
        audio_frames = meta['audio_sample_frames']
        audio_channels = meta.get('audio_channels')
        audio_rate = meta.get('audio_sample_rate')
        audio_file = target / 'audio.s16le'
        if audio_bytes < 0 or audio_frames < 0:
            raise RuntimeError('negative candidate audio byte/frame count')
        if audio_bytes:
            if not isinstance(audio_channels, int) or audio_channels <= 0:
                raise RuntimeError('candidate audio payload has invalid/missing channel count')
            if audio_bytes != audio_frames * audio_channels * 2:
                raise RuntimeError('candidate audio byte count does not match sample_frames * channels * 2')
            if not audio_file.is_file() or audio_file.stat().st_size != audio_bytes:
                raise RuntimeError('candidate audio.s16le size does not match metadata')
        elif audio_frames != 0:
            raise RuntimeError('candidate audio frames reported with zero audio bytes')
        vf = f'trim=start_frame={args.start_frame}:end_frame={args.start_frame + count},setpts=PTS-STARTPTS'
        oracle_cmd = [str(ffmpeg), '-nostdin', '-v', 'error', '-threads', '1', '-i',
                      str(input_path), '-map', '0:v:0', '-vf', vf, '-vsync', '0',
                      '-frames:v', str(count), '-pix_fmt', 'yuv420p', '-f', 'rawvideo', '-']
        oracle = run(oracle_cmd, timeout=300)
        actual = yuv.read_bytes()
        differences = sum(a != b for a, b in zip(actual, oracle)) + abs(len(actual) - len(oracle))
        max_abs = max((abs(a - b) for a, b in zip(actual, oracle)), default=0)
        video_pass = len(actual) == len(oracle) and differences == 0
        audio_observation = {'status': 'INCONCLUSIVE: audio prefix observation not requested'}
        if audio_bytes == 0 and meta['audio_tracks_in_header'] == 0:
            audio_observation = {'status': 'NOT_APPLICABLE'}
        elif args.audio_prefix and audio_bytes > 0:
            if not isinstance(audio_rate, int) or audio_rate <= 0:
                raise RuntimeError('candidate audio payload has invalid/missing sample rate')
            audio_cmd = [str(ffmpeg), '-nostdin', '-v', 'error', '-threads', '1',
                         '-i', str(input_path), '-map', '0:a:0', '-af',
                         f'atrim=end_sample={audio_frames}', '-ar', str(audio_rate),
                         '-ac', str(audio_channels), '-c:a', 'pcm_s16le', '-f', 's16le', '-']
            oracle_audio = run(audio_cmd, timeout=300)
            candidate_audio = audio_file.read_bytes()
            # Compare only the common complete signed-16 samples; preserve lengths separately.
            usable = min(len(candidate_audio), len(oracle_audio)) // 2 * 2
            cand = array.array('h'); cand.frombytes(candidate_audio[:usable])
            ref = array.array('h'); ref.frombytes(oracle_audio[:usable])
            if sys.byteorder != 'little':
                cand.byteswap(); ref.byteswap()
            deltas = [abs(a - b) for a, b in zip(cand, ref)]
            audio_observation = {
                'status': 'OBSERVED', 'oracle_command': audio_cmd,
                'candidate_sample_rate': audio_rate, 'candidate_channels': audio_channels,
                'sample_rate_and_channels_independently_verified': False,
                'candidate_sample_frames': audio_frames,
                'candidate_audio_bytes': len(candidate_audio),
                'oracle_audio_bytes': len(oracle_audio),
                'candidate_samples': len(candidate_audio) // 2,
                'oracle_samples': len(oracle_audio) // 2,
                'overlapping_samples_compared': len(deltas),
                'length_mismatch': len(candidate_audio) != len(oracle_audio),
                'differing_samples': sum(a != b for a, b in zip(cand, ref)),
                'max_abs_difference': max(deltas, default=0),
                'rms_difference': math.sqrt(sum(x * x for x in deltas) / len(deltas)) if deltas else 0.0,
                'acceptance': 'observation only; packet alignment and sample-count correctness unverified'}
        result.update(status='PASS' if video_pass else 'FAIL', video_pass=video_pass,
                      oracle_command=oracle_cmd, candidate_video_sha256=sha(yuv),
                      oracle_video_sha256=hashlib.sha256(oracle).hexdigest(),
                      candidate_video_bytes=len(actual), oracle_video_bytes=len(oracle),
                      differing_bytes=differences, max_abs_difference=max_abs,
                      decoded_frame_count=count,
                      alpha_status='candidate-exposed; ffmpeg alpha parity not assessed' if meta['has_alpha'] else 'not exposed',
                      audio_status=audio_observation['status'], audio_observation=audio_observation,
                      candidate_audio_bytes=audio_bytes)
    except Exception as exc:
        result.update(status='FAIL', failure=str(exc))
    return result


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', required=True, type=Path)
    p.add_argument('--input', required=True, action='append', type=Path)
    p.add_argument('--output-dir', required=True, type=Path)
    p.add_argument('--max-frames', type=int, default=16)
    p.add_argument('--start-frame', type=int, default=0)
    p.add_argument('--audio-prefix', action='store_true',
                   help='record an ffmpeg PCM prefix comparison; observation only')
    p.add_argument('--ffmpeg', default='ffmpeg')
    args = p.parse_args(argv)
    if args.max_frames <= 0 or args.start_frame < 0:
        p.error('--max-frames must be positive and --start-frame nonnegative')
    candidate = args.candidate.resolve()
    output = args.output_dir.resolve()
    inputs = [path.resolve() for path in args.input]
    if not candidate.is_file() or not os.access(candidate, os.X_OK):
        p.error(f'candidate must be an executable file: {candidate}')
    if any(not path.is_file() for path in inputs):
        p.error('every --input must name an existing file')
    if output.exists():
        p.error(f'output directory already exists: {output}')
    ffmpeg = shutil.which(args.ffmpeg)
    if ffmpeg is None:
        p.error(f'ffmpeg not found: {args.ffmpeg}')
    output.mkdir(parents=True)
    receipt = {'scope': 'video_prefix_only', 'acceptance_passed': False,
               'candidate': str(candidate), 'candidate_sha256': sha(candidate),
               'candidate_version': None, 'ffmpeg': ffmpeg,
               'ffmpeg_version': None, 'limits': {'max_frames': args.max_frames,
                                                  'start_frame': args.start_frame,
                                                  'audio_prefix_observation': args.audio_prefix},
               'results': []}
    try:
        receipt['candidate_version'] = run([str(candidate), '--version'], timeout=10).decode(errors='replace').strip()
    except Exception as exc:
        receipt['candidate_version'] = f'unavailable: {exc}'
    try:
        receipt['ffmpeg_version'] = run([ffmpeg, '-version'], timeout=10).decode(errors='replace').splitlines()[0]
    except Exception as exc:
        receipt['ffmpeg_version'] = f'unavailable: {exc}'
    for index, input_path in enumerate(inputs):
        case = output / f'case-{index:03d}'
        case.mkdir()
        receipt['results'].append(compare(case, candidate, ffmpeg, input_path, args))
    receipt['passed'] = all(item['status'] == 'PASS' for item in receipt['results'])
    receipt_path = output / 'receipt.json'
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'passed': receipt['passed'], 'acceptance_passed': False,
                      'scope': receipt['scope'], 'cases': len(receipt['results']),
                      'receipt': str(receipt_path)}, indent=2))
    return 0 if receipt['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
