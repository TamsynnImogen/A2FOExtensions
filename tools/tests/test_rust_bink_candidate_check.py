import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools import check_rust_bink_candidate as harness


class HarnessTests(unittest.TestCase):
    def test_video_byte_mismatch_fails_and_reports_metrics(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            candidate = root / 'candidate'
            candidate.write_text('')
            source = root / 'input.bik'
            source.write_bytes(b'fixture')
            output = root / 'oracle.yuv'
            output.write_bytes(b'\x01' * 6)
            case = root / 'case'
            case.mkdir()
            target = case / 'candidate'
            target.mkdir()
            (target / 'video.yuv').write_bytes(b'\x00' * 6)
            (target / 'metadata.json').write_text(json.dumps({
                'signature': 'BIK', 'width': 2, 'height': 2, 'fps_num': 30,
                'fps_den': 1, 'source_frame_count': 1, 'decoded_frame_count': 1,
                'pixel_format': 'yuv420p', 'chroma_width': 1, 'chroma_height': 1,
                'has_alpha': False, 'audio_tracks_in_header': 0,
                'audio_sample_rate': None, 'audio_channels': None,
                'audio_sample_frames': 0, 'video_bytes': 6, 'alpha_bytes': 0,
                'audio_bytes': 0, 'start_frame': 0}))
            args = type('Args', (), {'max_frames': 16, 'start_frame': 0,
                                     'audio_prefix': False})()
            with mock.patch.object(harness, 'run', return_value=output.read_bytes()):
                result = harness.compare(case, candidate, Path('ffmpeg'), source, args)
            self.assertEqual(result['status'], 'FAIL')
            self.assertEqual(result['differing_bytes'], 6)
            self.assertEqual(result['max_abs_difference'], 1)
            self.assertEqual(result['audio_status'], 'NOT_APPLICABLE')

    def test_audio_prefix_records_numeric_diff_and_unequal_length(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            candidate = root / 'candidate'
            candidate.write_text('')
            source = root / 'input.bik'
            source.write_bytes(b'fixture')
            case = root / 'case'
            case.mkdir()
            target = case / 'candidate'
            target.mkdir()
            (target / 'video.yuv').write_bytes(b'\x00' * 6)
            (target / 'audio.s16le').write_bytes((0).to_bytes(2, 'little', signed=True) +
                                                 (2).to_bytes(2, 'little', signed=True))
            (target / 'metadata.json').write_text(json.dumps({
                'signature': 'BIK', 'width': 2, 'height': 2, 'fps_num': 30,
                'fps_den': 1, 'source_frame_count': 1, 'decoded_frame_count': 1,
                'pixel_format': 'yuv420p', 'chroma_width': 1, 'chroma_height': 1,
                'has_alpha': False, 'audio_tracks_in_header': 1,
                'audio_sample_rate': 22050, 'audio_channels': 1,
                'audio_sample_frames': 2, 'video_bytes': 6, 'alpha_bytes': 0,
                'audio_bytes': 4, 'start_frame': 0}))
            args = type('Args', (), {'max_frames': 16, 'start_frame': 0,
                                     'audio_prefix': True})()
            oracle_video = b'\x00' * 6
            oracle_audio = ((1).to_bytes(2, 'little', signed=True) +
                            (2).to_bytes(2, 'little', signed=True) +
                            (99).to_bytes(2, 'little', signed=True))
            with mock.patch.object(harness, 'run', side_effect=[b'', oracle_video, oracle_audio]):
                result = harness.compare(case, candidate, Path('ffmpeg'), source, args)
            self.assertIn('audio_observation', result, result)
            observed = result['audio_observation']
            self.assertEqual(result['status'], 'PASS')
            self.assertEqual(observed['differing_samples'], 1)
            self.assertEqual(observed['max_abs_difference'], 1)
            self.assertEqual(observed['oracle_samples'], 3)
            self.assertTrue(observed['length_mismatch'])
            self.assertIn('observation only', observed['acceptance'])

    def test_missing_candidate_output_is_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            candidate = root / 'candidate'
            candidate.write_text('')
            source = root / 'input.bik'
            source.write_bytes(b'fixture')
            case = root / 'case'
            case.mkdir()
            args = type('Args', (), {'max_frames': 16, 'start_frame': 0,
                                     'audio_prefix': False})()
            with mock.patch.object(harness, 'run', return_value=b''):
                result = harness.compare(case, candidate, Path('ffmpeg'), source, args)
            self.assertEqual(result['status'], 'FAIL')
            self.assertIn('metadata.json', result['failure'])


if __name__ == '__main__':
    unittest.main()
