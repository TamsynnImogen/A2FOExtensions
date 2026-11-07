"""Headless HTTP checks for the read-only Native11 snapshot viewer."""

import http.client
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import native11_live_view as view


def bmp(width=2, height=2):
    stride = (width * 3 + 3) & ~3
    pixels = b'\0' * (stride * height)
    return (b'BM' + struct.pack('<III', 54 + len(pixels), 0, 54) +
            struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 24, 0,
                        len(pixels), 0, 0, 0, 0) + pixels)


def status(**changes):
    obj = {'state': 'frame', 'reason': '', 'serial': '1', 'frame': '3',
           'source_epoch': '2', 'source_ticket': 1, 'objects': 1,
           'width': 2, 'height': 2, 'slot': 'a'}
    obj.update(changes)
    return json.dumps(obj).encode()


class LiveViewHTTPTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.server = view.ThreadingHTTPServer(('127.0.0.1', 0), view.make_handler(self.root))
        self.addCleanup(self.server.server_close)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_server)

    def stop_server(self):
        self.server.shutdown()
        self.thread.join(timeout=2)

    def get(self, path):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=2)
        conn.request('GET', path)
        response = conn.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        conn.close()
        return result

    def publish(self, raw=None, frame=None):
        (self.root / 'status.json').write_bytes(status() if raw is None else raw)
        (self.root / 'frame-a.bmp').write_bytes(bmp() if frame is None else frame)

    def test_missing_and_stale_status(self):
        code, headers, body = self.get('/status.json')
        self.assertEqual(code, 200)
        self.assertEqual(headers['Cache-Control'], 'no-store')
        self.assertEqual(json.loads(body)['state'], 'waiting')
        self.publish()
        old = time.time() - 6
        os.utime(self.root / 'status.json', (old, old))
        obj = json.loads(self.get('/status.json')[2])
        self.assertTrue(obj['stale'])
        self.assertGreater(obj['age_seconds'], 5)
        self.assertEqual(self.get('/frame.bmp?serial=1')[0], 409)
        code, _, body = self.get('/')
        self.assertEqual(code, 200)
        self.assertIn(b'textContent', body)
        self.assertIn(b'!s.stale', body)

    def test_good_frame_and_serial_conflict(self):
        self.publish()
        code, headers, body = self.get('/frame.bmp?serial=1')
        self.assertEqual(code, 200)
        self.assertEqual(headers['Content-Type'], 'image/bmp')
        self.assertEqual(body, bmp())
        self.assertEqual(self.get('/frame.bmp?serial=2')[0], 409)

    def test_paths_and_symlinks(self):
        self.publish()
        for path in ('/status.json/../frame-a.bmp', '/frame-a.bmp', '/frame.bmp?serial=1&x=2',
                     '/frame.bmp?serial=../1', '/%2e%2e/status.json'):
            self.assertEqual(self.get(path)[0], 404, path)
        (self.root / 'status.json').unlink()
        (self.root / 'status.json').symlink_to(self.root / 'frame-a.bmp')
        self.assertEqual(json.loads(self.get('/status.json')[2])['state'], 'unavailable')
        self.assertEqual(self.get('/frame.bmp?serial=1')[0], 422)
        (self.root / 'status.json').unlink()
        (self.root / 'status.json').write_bytes(status())
        (self.root / 'frame-a.bmp').unlink()
        (self.root / 'frame-a.bmp').symlink_to(self.root / 'status.json')
        self.assertEqual(self.get('/frame.bmp?serial=1')[0], 422)

    def test_export_root_symlink_rejected(self):
        self.publish()
        linked = self.root.parent / (self.root.name + '-link')
        linked.symlink_to(self.root, target_is_directory=True)
        self.addCleanup(linked.unlink)
        with self.assertRaises(view.InvalidExport):
            view._read_leaf(linked, 'status.json', view.MAX_JSON)

    def test_malformed_status(self):
        for raw in (b'{', b' ' * (view.MAX_JSON + 1), status(serial=1),
                    status(slot='../a'), status(reason='x' * 513),
                    status(width=4097), status(state='waiting')):
            self.publish(raw=raw)
            self.assertEqual(json.loads(self.get('/status.json')[2])['state'], 'unavailable')

    def test_escaped_codec_reason_is_plain_text(self):
        reason = 'texture_\u00e9\n<script>literal</script>'
        self.publish(raw=status(reason=reason))
        self.assertEqual(json.loads(self.get('/status.json')[2])['reason'], reason)

    def test_bad_bmp(self):
        for frame in (bmp()[:-1], bmp() + b'X', b'BM' + b'\0' * (view.MAX_BMP + 1),
                      bmp(width=3), bmp().replace(b'BM', b'ZZ', 1)):
            self.publish(frame=frame)
            self.assertEqual(self.get('/frame.bmp?serial=1')[0], 422)

    def test_publication_change_during_frame_read(self):
        self.publish()
        original = view._read_leaf

        def change(directory, name, limit):
            result = original(directory, name, limit)
            if name == 'frame-a.bmp':
                (self.root / 'status.json').write_bytes(status(serial='2'))
            return result

        with patch.object(view, '_read_leaf', side_effect=change):
            self.assertEqual(self.get('/frame.bmp?serial=1')[0], 409)


if __name__ == '__main__':
    unittest.main()
