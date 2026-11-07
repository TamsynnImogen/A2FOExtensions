import hashlib
import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "renderer_shader_report.py"
SPEC = importlib.util.spec_from_file_location("renderer_shader_report", SCRIPT)
reporter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(reporter)


class RendererShaderReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def trace(self, name, rows):
        path = self.root / name
        path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
        return path

    def shader(self, function=None, declaration=None):
        return {
            "event": "shader", "capture": 1, "frame": 0, "object": 2,
            "stage": "vertex", "handle": 4097, "usage": 0,
            "declaration": declaration if declaration is not None else {"status": "complete", "words": ["0x20000000", "0xffffffff"]},
            "function": function if function is not None else {"status": "complete", "words": ["0xfffe0101", "0x0000ffff"]},
        }

    def test_hashes_complete_payload_as_little_endian_dwords_and_keeps_version(self):
        words = [0xFFFE0101, 0x0000FFFF]
        row = self.shader()
        got = reporter.summarize_trace(self.trace("capture.jsonl", [{"event": "session", "schema": 2, "pid": 12}, row]))
        shader = got["shaders"][0]
        expected = hashlib.sha256(b"".join(struct.pack("<I", value) for value in words)).hexdigest()
        self.assertEqual(shader["function"]["sha256"], expected)
        self.assertEqual(shader["function"]["version"], "1.1")
        self.assertEqual(shader["declaration"]["word_count"], 2)
        self.assertNotEqual(shader["function"]["sha256"], hashlib.sha256(b"".join(struct.pack(">I", value) for value in words)).hexdigest())

    def test_complete_opaque_def_payload_end_bits_and_changed_token_change_hash(self):
        words = ["0xfffe0101", "0x05000051", "0xaabbccdd", "0x0000ffff"]
        changed = [*words[:-2], "0xaabbccde", words[-1]]
        first = self.shader(function={"status": "complete", "words": words})
        second = self.shader(function={"status": "complete", "words": changed})
        reports = reporter.summarize_trace(self.trace("def.jsonl", [first, second]))["shaders"]
        self.assertEqual(reports[0]["function"]["word_count"], 4)
        self.assertTrue(reports[0]["function"]["sha256"])
        self.assertNotEqual(reports[0]["function"]["sha256"], reports[1]["function"]["sha256"])

    def test_vertex_declaration_can_have_absent_function(self):
        row = self.shader(function={"status": "absent", "words": []})
        result = reporter.summarize_trace(self.trace("declaration.jsonl", [row]))["shaders"][0]
        self.assertEqual(result["stage"], "vertex")
        self.assertEqual(result["function"], {"status": "absent", "word_count": 0})

    def test_empty_incomplete_payload_is_not_hashed(self):
        row = self.shader(function={"status": "word_limit", "words": []})
        result = reporter.summarize_trace(self.trace("partial.jsonl", [row]))
        function = result["shaders"][0]["function"]
        self.assertEqual(function, {"status": "word_limit", "word_count": 0})
        self.assertEqual(result["shader_contents"], "unknown")

    def test_method_count_snapshots_use_latest_and_max_not_sum(self):
        rows = [
            {"event": "count", "method": "IDirect3DDevice8.CreateVertexShader", "calls": 1},
            {"event": "count", "method": "IDirect3DDevice8.CreateVertexShader", "calls": 4},
            {"event": "count", "method": "IDirect3DDevice8.CreateVertexShader", "calls": 4},
        ]
        counts = reporter.summarize_trace(self.trace("count-snapshots.jsonl", rows))["shader_method_calls"]["CreateVertexShader"]
        self.assertEqual(counts["latest_method_calls"], 4)
        self.assertEqual(counts["max_method_calls"], 4)

    def test_signature_snapshots_remain_separate_and_keyed(self):
        rows = [
            {"event": "signature", "key": "IDirect3DDevice8.CreateVertexShader|x", "calls": 8},
            {"event": "signature", "key": "IDirect3DDevice8.CreateVertexShader|y", "calls": 3},
            {"event": "signature", "key": "IDirect3DDevice8.CreateVertexShader|x", "calls": 2},
        ]
        counts = reporter.summarize_trace(self.trace("counts.jsonl", rows))["shader_method_calls"]["CreateVertexShader"]
        self.assertIsNone(counts["latest_method_calls"])
        self.assertEqual(counts["signature_snapshots"][rows[0]["key"]], {"latest_calls": 2, "max_calls": 8})

    def test_schema_one_pointer_only_trace_reports_unknown_contents(self):
        result = reporter.summarize_trace(self.trace("old.jsonl", [{"event": "session", "schema": 1, "pid": 2}]))
        self.assertEqual(result["shader_capture_count"], 0)
        self.assertEqual(result["shader_contents"], "unknown")

    def test_malformed_noncomplete_and_oversized_payloads_name_file_and_line(self):
        malformed_rows = ["0x123", "0x+0000001", "0x 0000001"]
        for index, word in enumerate(malformed_rows):
            malformed = self.trace(f"bad-{index}.jsonl", [self.shader(function={"status": "complete", "words": [word]})])
            with self.assertRaisesRegex(reporter.TraceError, rf"bad-{index}\.jsonl:1:.*invalid DWORD"):
                reporter.summarize_trace(malformed)
        partial = self.trace("partial-words.jsonl", [self.shader(function={"status": "unreadable", "words": ["0x00000000"]})])
        with self.assertRaisesRegex(reporter.TraceError, r"partial-words\.jsonl:1:.*must not contain words"):
            reporter.summarize_trace(partial)
        too_many = ["0x00000000"] * 4097
        oversized = self.trace("large.jsonl", [self.shader(function={"status": "unsupported", "words": too_many})])
        with self.assertRaisesRegex(reporter.TraceError, r"large\.jsonl:1:.*exceeds 4096 words"):
            reporter.summarize_trace(oversized)


if __name__ == "__main__":
    unittest.main()
