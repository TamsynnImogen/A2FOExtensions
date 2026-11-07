from pathlib import Path
import tempfile
import unittest

from tools.check_rust_bink_native_audio import compare_results


class NativeAudioParityTests(unittest.TestCase):
    def compare(self, root, native_bits, native_pcm_bytes, candidate_samples,
                native_frame_bytes=None):
        candidate_pcm = root / 'candidate.pcm'
        native_pcm = root / 'native.bin'
        candidate_pcm.write_bytes(b''.join(int(x).to_bytes(2, 'little', signed=True)
                                            for x in candidate_samples))
        native_pcm.write_bytes(native_pcm_bytes)
        if native_frame_bytes is None:
            native_frame_bytes = len(native_pcm_bytes)
        candidate = {
            'audio_tracks_in_header': 1, 'audio_sample_rate': 22050,
            'audio_channels': 1, 'decoded_frame_count': 1,
            'per_frame_audio_sample_frames': [len(candidate_samples)],
            'audio_bytes': candidate_pcm.stat().st_size,
        }
        native = {'track_prefix': [22050, native_bits, 1, 4096],
                  'open_prefix': [16, 16, 1],
                  'frames': [{'audio_bytes': native_frame_bytes}]}
        return compare_results(candidate, candidate_pcm, native, native_pcm)

    def test_16bit_pcm_delta_one_meets_fixed_threshold(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.compare(Path(temp), 16,
                                  (100).to_bytes(2, 'little', signed=True), [101])
            self.assertEqual(result['pcm']['max_abs_difference'], 1)
            self.assertEqual(result['pcm']['differing_samples'], 1)

    def test_16bit_pcm_delta_two_fails_fixed_threshold(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.compare(Path(temp), 16,
                                  (100).to_bytes(2, 'little', signed=True), [102])
            self.assertFalse(result['passed'])
            self.assertIn('exceeds fixed criterion 1', result['failures'][0])

    def test_8bit_unsigned_native_is_normalized_and_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.compare(Path(temp), 8, bytes([128]), [0])
            self.assertEqual(result['pcm']['max_abs_difference'], 0)
            self.assertEqual(result['pcm']['comparison_units'], 'native unsigned 8-bit codes')

    def test_8bit_threshold_uses_native_codes_and_keeps_raw16_diagnostic(self):
        with tempfile.TemporaryDirectory() as temp:
            # 1000 >> 8 + 128 == 131: exact native code match, though 16-bit
            # normalization differs by 232 units and remains recorded.
            result = self.compare(Path(temp), 8, bytes([131]), [1000])
            self.assertTrue(result['passed'])
            self.assertEqual(result['pcm']['max_abs_difference'], 0)
            self.assertEqual(result['pcm']['raw16_normalized_diagnostic']['max_abs_difference'], 232)

    def test_8bit_quantization_clips_signed_16_edges(self):
        with tempfile.TemporaryDirectory() as temp:
            samples = [-32768, -32767, -1, 0, 1, 32767]
            result = self.compare(Path(temp), 8,
                                  bytes([0, 0, 127, 128, 128, 255]), samples)
            self.assertTrue(result['passed'])
            self.assertEqual(result['pcm']['max_abs_difference'], 0)

    def test_unequal_whole_pcm_lengths_are_recorded_as_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.compare(Path(temp), 16,
                                  (100).to_bytes(2, 'little', signed=True), [100, 100],
                                  native_frame_bytes=2)
            self.assertFalse(result['passed'])
            self.assertFalse(result['pcm']['sample_count_match'])
            self.assertEqual(result['pcm']['compared_samples'], 1)


if __name__ == '__main__':
    unittest.main()
