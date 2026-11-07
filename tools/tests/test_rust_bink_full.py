import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools import check_rust_bink_full as harness


def executable(path, source):
    path.write_text('#!/usr/bin/env python3\n' + source)
    path.chmod(0o755)
    return path


class FullHashHarnessTests(unittest.TestCase):
    def fixture(self, root, oracle_hash):
        input_path = root / 'input.bik'
        input_path.write_bytes(b'fixture input')
        scan_hash = hashlib.sha256(b'video pixels').hexdigest()
        scanner = executable(root / 'scanner', f'''import json, pathlib, sys
if sys.argv[1:] == ['--version']:
    print('test scanner 1')
    raise SystemExit(0)
source, output = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
memory = len(sys.argv) == 4
json.dump({{'video_sha256': '{scan_hash}', 'video_bytes': 12,
           'elapsed_seconds': 1.0 if not memory else 2.0,
           'scan_mode': 'memory' if memory else 'file'}}, output.open('w'))
''')
        ffmpeg = executable(root / 'ffmpeg', f'''import sys
if '-version' in sys.argv:
    print('test ffmpeg')
else:
    print('SHA256={oracle_hash}')
''')
        ffprobe = executable(root / 'ffprobe', '''import json, sys
if '-version' in sys.argv:
    print('test ffprobe')
else:
    print(json.dumps({'streams': [{'codec_type': 'video', 'r_frame_rate': '30/1'}]}))
''')
        return input_path, scanner, ffmpeg, ffprobe, scan_hash

    def test_file_memory_hash_parity_ignores_only_mode_and_elapsed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            scan_hash = hashlib.sha256(b'video pixels').hexdigest()
            input_path, scanner, ffmpeg, ffprobe, _ = self.fixture(root, scan_hash)
            case_root = root / 'out'
            case_root.mkdir()
            result = harness.case_worker(
                0, {'input': str(input_path), 'sha256': harness.digest(input_path)},
                scanner, ffmpeg, ffprobe, case_root, True)
            self.assertEqual(result['status'], 'PASS', result)
            self.assertTrue(result['file_memory_match'])
            self.assertTrue(result['input_unchanged'])
            self.assertEqual(result['file_memory_differences'], {})

    def test_full_video_digest_mismatch_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            input_path, scanner, ffmpeg, ffprobe, _ = self.fixture(root, '0' * 64)
            case_root = root / 'out'
            case_root.mkdir()
            result = harness.case_worker(
                0, {'input': str(input_path), 'sha256': harness.digest(input_path)},
                scanner, ffmpeg, ffprobe, case_root, False)
            self.assertEqual(result['status'], 'FAIL')
            self.assertIn('SHA-256 mismatch', result['failure'])


if __name__ == '__main__':
    unittest.main()
