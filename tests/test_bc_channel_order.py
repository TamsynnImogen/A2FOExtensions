import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "build_bc_texture_mod.py"
SPEC = importlib.util.spec_from_file_location("build_bc_texture_mod", SCRIPT)
converter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(converter)


class BCChannelOrderTests(unittest.TestCase):
    def test_rgb24_masks_expand_to_bgra8_without_mixing_authored_mips(self):
        header = bytearray(128)
        header[:4] = b"DDS "
        struct.pack_into("<I", header, 4, 124)
        struct.pack_into("<I", header, 8, 0x2100F)
        struct.pack_into("<II", header, 12, 4, 8)
        struct.pack_into("<I", header, 20, 24)
        struct.pack_into("<I", header, 28, 2)
        struct.pack_into("<I", header, 76, 32)
        struct.pack_into("<I", header, 80, 0x40)
        struct.pack_into("<I", header, 88, 24)
        struct.pack_into("<IIII", header, 92, 0xFF0000, 0xFF00, 0xFF, 0)
        # Stored BGR bytes describe red in mip 0 and blue in mip 1.
        payload = bytes((7, 23, 211)) * 32 + bytes((197, 31, 5)) * 8
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "authored.dds"
            source.write_bytes(header + payload)
            transform, result = converter.canonical_bgra8_source(source, 8, 4, 2)
            self.assertEqual(transform, "rgb24-bgra8-v2")
            self.assertEqual(struct.unpack_from("<I", result, 88)[0], 32)
            self.assertEqual(struct.unpack_from("<IIII", result, 92),
                             (0xFF0000, 0xFF00, 0xFF, 0xFF000000))
            self.assertEqual(converter.source_mips(result, 8, 4, 2), [
                (8, 4, bytes((7, 23, 211, 255)) * 32),
                (4, 2, bytes((197, 31, 5, 255)) * 8),
            ])

    def test_rgb24_rejects_truncated_payload(self):
        header = bytearray(128)
        header[:4] = b"DDS "
        struct.pack_into("<I", header, 80, 0x40)
        struct.pack_into("<I", header, 88, 24)
        struct.pack_into("<IIII", header, 92, 0xFF0000, 0xFF00, 0xFF, 0)
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "short.dds"
            source.write_bytes(header + bytes(11))
            with self.assertRaisesRegex(ValueError, "exact tightly packed mip payload"):
                converter.canonical_bgra8_source(source, 2, 2, 1)

    def test_bgra32_keeps_existing_codec_path(self):
        header = bytearray(128)
        header[:4] = b"DDS "
        struct.pack_into("<I", header, 80, 0x41)
        struct.pack_into("<I", header, 88, 32)
        struct.pack_into("<IIII", header, 92, 0xFF0000, 0xFF00, 0xFF, 0xFF000000)
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "bgra.dds"
            source.write_bytes(header + bytes((7, 23, 211, 255)) * 16)
            self.assertIsNone(converter.canonical_bgra8_source(source, 4, 4, 1))

    def test_authored_6x6_rgb24_mip_replicates_edges_without_changing_block_count(self):
        header = bytearray(128)
        header[:4] = b"DDS "
        struct.pack_into("<I", header, 4, 124)
        struct.pack_into("<II", header, 12, 6, 6)
        struct.pack_into("<I", header, 28, 2)
        struct.pack_into("<I", header, 76, 32)
        struct.pack_into("<I", header, 80, 0x40)
        struct.pack_into("<I", header, 88, 24)
        struct.pack_into("<IIII", header, 92, 0xFF0000, 0xFF00, 0xFF, 0)
        top = b"".join(bytes((x, y, x + y)) for y in range(6) for x in range(6))
        authored_lower = bytes((199, 61, 17)) * 9
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "authored.dds"
            source.write_bytes(header + top + authored_lower)
            transform, canonical = converter.canonical_bgra8_source(source, 6, 6, 2)
            mips = converter.source_mips(canonical, 6, 6, 2)
        self.assertEqual(transform, "rgb24-bgra8-v2")
        self.assertTrue(converter.rgb24_bc7_needs_padding(6, 6, 2))
        self.assertFalse(converter.rgb24_bc7_needs_padding(8, 8, 2))
        self.assertTrue(converter.rgb24_bc7_needs_padding(8, 8, 4))
        width, height, padded = converter.pad_bgra8_to_bc7_blocks(*mips[0])
        self.assertEqual((width, height), (8, 8))
        for y in range(8):
            for x in range(8):
                original_x, original_y = min(x, 5), min(y, 5)
                self.assertEqual(padded[(y * 8 + x) * 4:(y * 8 + x + 1) * 4],
                                 bytes((original_x, original_y, original_x + original_y, 255)))
        self.assertEqual(len(padded), 8 * 8 * 4)
        lower_width, lower_height, lower_padded = converter.pad_bgra8_to_bc7_blocks(*mips[1])
        self.assertEqual((lower_width, lower_height), (4, 4))
        self.assertEqual(lower_padded, bytes((199, 61, 17, 255)) * 16)
        self.assertEqual(mips[1][2], bytes((199, 61, 17, 255)) * 9)
        tiny = bytes((1, 2, 3, 255, 4, 5, 6, 255))
        self.assertEqual(converter.pad_bgra8_to_bc7_blocks(1, 2, tiny),
                         (4, 4, tiny[:4] * 4 + tiny[4:] * 12))
        self.assertEqual(((6 + 3) // 4) ** 2 * 16, ((8 + 3) // 4) ** 2 * 16)

    def test_padded_bc7_mip_header_restores_original_dimensions(self):
        header = bytearray(148)
        header[:4] = b"DDS "
        struct.pack_into("<I", header, 4, 124)
        struct.pack_into("<I", header, 8, 0x81007)
        struct.pack_into("<II", header, 12, 8, 8)
        struct.pack_into("<I", header, 20, 64)
        struct.pack_into("<I", header, 28, 1)
        struct.pack_into("<I", header, 76, 32)
        struct.pack_into("<I", header, 80, 4)
        header[84:88] = b"DX10"
        struct.pack_into("<IIII", header, 128, 98, 3, 0, 1)
        encoded = bytes(header) + bytes(64)
        entry = {"width": 8, "height": 8, "mips": 1, "target_format": "BC7"}
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "codec.dds"
            output.write_bytes(encoded)
            converter.validate_output(output, entry)
            restored = converter.restore_encoded_mip_dimensions(output.read_bytes(), 6, 6)
            output.write_bytes(restored)
            converter.validate_output(output, dict(entry, width=6, height=6))
        self.assertEqual(converter.dds_header_bytes(restored)[:3], (6, 6, 1))
        self.assertEqual(len(restored), len(encoded))
        expected = bytearray(encoded)
        struct.pack_into("<II", expected, 12, 6, 6)
        self.assertEqual(restored, bytes(expected))


if __name__ == "__main__":
    unittest.main()
