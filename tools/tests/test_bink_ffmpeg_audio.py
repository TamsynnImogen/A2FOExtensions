from pathlib import Path
import struct
import tempfile
import unittest

from tools.check_rust_bink_ffmpeg_audio import (
    packet_sample_limits,
    parse_probe_rows,
    trim_packet_pcm,
)


def synthetic_audio_bik():
    frame_count, track_count = 2, 1
    index_start = 44 + 12 * track_count
    packet0, packet1, end = 68, 80, 92
    data = bytearray(end)
    data[:4] = b'BIKi'
    struct.pack_into('<I', data, 4, end - 8)
    struct.pack_into('<I', data, 8, frame_count)
    struct.pack_into('<I', data, 12, 16)
    struct.pack_into('<I', data, 16, frame_count)
    struct.pack_into('<I', data, 20, 16)
    struct.pack_into('<I', data, 24, 16)
    struct.pack_into('<I', data, 28, 30)
    struct.pack_into('<I', data, 32, 1)
    struct.pack_into('<I', data, 40, track_count)
    struct.pack_into('<I', data, 44, 8)
    struct.pack_into('<HH', data, 48, 22050, 0x4000)
    struct.pack_into('<III', data, index_start, packet0, packet1, end)
    struct.pack_into('<II', data, packet0, 8, 6)  # 3 mono s16 frames in packet 0
    struct.pack_into('<II', data, packet1, 8, 4)  # 2 mono s16 frames in packet 1
    return bytes(data)


class PacketAwareAudioTests(unittest.TestCase):
    def test_probe_rows_group_packet_samples_and_trim_packet_padding(self):
        groups = parse_probe_rows('72,2\n72,3\n84,4\n')
        self.assertEqual(groups, [
            {'pkt_pos': 72, 'decoded_sample_frames': 5},
            {'pkt_pos': 84, 'decoded_sample_frames': 4},
        ])
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            raw = root / 'raw.s16le'
            trimmed = root / 'trimmed.s16le'
            raw.write_bytes(struct.pack('<9h', *range(10, 19)))
            limits = {72: {'frame': 0, 'sample_frames': 3},
                      84: {'frame': 1, 'sample_frames': 2}}
            report = trim_packet_pcm(raw, trimmed, groups, limits, channels=1)
            self.assertEqual(struct.unpack('<5h', trimmed.read_bytes()), (10, 11, 12, 15, 16))
            self.assertEqual(report['ffmpeg_padding_sample_frames_removed'], 4)

    def test_packet_positions_map_to_prefix_reported_s16_frame_counts(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'two.bik'
            path.write_bytes(synthetic_audio_bik())
            frame_count, packets, per_frame = packet_sample_limits(path, channels=1)
            self.assertEqual(frame_count, 2)
            self.assertEqual(packets, {
                72: {'frame': 0, 'sample_frames': 3},
                84: {'frame': 1, 'sample_frames': 2},
            })
            self.assertEqual(per_frame, [3, 2])

    def test_packet_prefix_longer_than_ffmpeg_packet_is_a_failure(self):
        groups = [{'pkt_pos': 72, 'decoded_sample_frames': 2}]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            raw, trimmed = root / 'raw.s16le', root / 'trimmed.s16le'
            raw.write_bytes(struct.pack('<2h', 1, 2))
            with self.assertRaisesRegex(RuntimeError, 'ffmpeg decoded only 2'):
                trim_packet_pcm(raw, trimmed, groups,
                                {72: {'frame': 0, 'sample_frames': 3}}, channels=1)


if __name__ == '__main__':
    unittest.main()
