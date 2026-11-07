#!/usr/bin/env python3
"""Read-only loopback viewer for the native11 snapshot export directory."""

import argparse
import json
import os
from pathlib import Path
import re
import stat
import struct
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


MAX_JSON = 4096
MAX_BMP = 64 * 1024 * 1024
UINT64 = 2**64 - 1
HTML = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Native11 live preview</title><style>
:root{color-scheme:dark;font:16px system-ui;background:#101820;color:#eef4f8}
body{max-width:1100px;margin:2rem auto;padding:0 1rem}h1{margin-bottom:.3rem}
[hidden]{display:none!important}
p{color:#b9ccd8}.panel{background:#1d2b36;padding:1rem;border-radius:10px}
img{display:block;max-width:100%;height:auto;margin:auto;image-rendering:auto}
dl{display:grid;grid-template-columns:max-content 1fr;gap:.35rem 1rem}dt{color:#9cb5c4}
</style><h1>Observed DX11 experimental preview</h1>
<p>Ships, poses and materials from the latest capture. This is an incomplete scene: no shadows or full gameplay.</p>
<p>Default: one snapshot per second, with neutral ambient light.</p>
<div class="panel"><img id="frame" alt="Native11 capture" hidden><p id="placeholder">Waiting for capture</p></div>
<dl><dt>State</dt><dd id="state">waiting</dd><dt>Reason</dt><dd id="reason">-</dd>
<dt>Frame</dt><dd id="number">-</dd><dt>Age</dt><dd id="age">-</dd>
<dt>Resolution</dt><dd id="size">-</dd><dt>Objects</dt><dd id="objects">-</dd></dl>
<script>
const field=(id,value)=>{document.getElementById(id).textContent=String(value)};
let shown='';
async function poll(){
 try{
  const r=await fetch('/status.json',{cache:'no-store'});if(!r.ok)throw Error('status unavailable');
  const s=await r.json();field('state',s.state);field('reason',s.reason||'-');
  field('number',s.state==='frame'?s.frame:'-');
  field('age',s.age_seconds==null?'-':s.age_seconds.toFixed(1)+' s');
  field('size',s.state==='frame'?s.width+' x '+s.height:'-');
  field('objects',s.state==='frame'?s.objects:'-');
  const visible=s.state==='frame'&&!s.stale;
  document.getElementById('frame').hidden=!visible;
  document.getElementById('placeholder').hidden=visible;
  if(!visible){field('placeholder',s.stale?'Capture stale':s.reason||'Waiting for capture');shown='';return}
  if(shown!==s.serial){shown=s.serial;document.getElementById('frame').src='/frame.bmp?serial='+encodeURIComponent(s.serial)}
 }catch(e){document.getElementById('frame').hidden=true;document.getElementById('placeholder').hidden=false;
  field('placeholder','Status unavailable');field('state','unavailable');shown=''}
}
poll();setInterval(poll,500);
</script></html>'''.encode('utf-8')


class InvalidExport(ValueError):
    pass


def _read_leaf(directory, name, limit):
    current = Path(directory.anchor)
    for part in directory.parts[1:]:
        current /= part
        if stat.S_ISLNK(os.lstat(current).st_mode):
            raise InvalidExport('export directory path contains a symlink')
    root = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=root)
        try:
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
                raise InvalidExport('invalid export file size or type')
            data = os.read(fd, limit + 1)
            if len(data) != info.st_size:
                raise InvalidExport('export file changed during read')
            return data, info.st_mtime
        finally:
            os.close(fd)
    finally:
        os.close(root)


def _uint_string(value):
    return isinstance(value, str) and re.fullmatch(r'(0|[1-9][0-9]*)', value) is not None and int(value) <= UINT64


def parse_status(raw):
    try:
        obj = json.loads(raw)
    except (ValueError, UnicodeDecodeError) as exc:
        raise InvalidExport('invalid status JSON') from exc
    required = {'state', 'reason', 'serial', 'frame', 'source_epoch', 'source_ticket', 'objects', 'width', 'height'}
    if not isinstance(obj, dict) or set(obj) not in (required, required | {'slot'}):
        raise InvalidExport('invalid status fields')
    if obj['state'] not in ('waiting', 'unavailable', 'frame', 'stopped'):
        raise InvalidExport('invalid state')
    reason = obj['reason']
    if not isinstance(reason, str) or len(reason) > 512:
        raise InvalidExport('invalid reason')
    if not all(_uint_string(obj[k]) for k in ('serial', 'frame', 'source_epoch')):
        raise InvalidExport('invalid counters')
    for key in ('source_ticket', 'objects', 'width', 'height'):
        if type(obj[key]) is not int or not 0 <= obj[key] <= 2**32 - 1:
            raise InvalidExport('invalid numeric field')
    if obj['state'] == 'frame':
        if obj.get('slot') not in ('a', 'b') or not 1 <= obj['width'] <= 4096 or not 1 <= obj['height'] <= 4096:
            raise InvalidExport('invalid frame status')
    elif 'slot' in obj or obj['width'] or obj['height']:
        raise InvalidExport('invalid nonframe status')
    return obj


def validate_bmp(data, width, height):
    if len(data) < 54 or len(data) > MAX_BMP or data[:2] != b'BM':
        raise InvalidExport('invalid BMP header')
    size, reserved, offset = struct.unpack_from('<III', data, 2)
    dib, bmp_width, bmp_height, planes, bpp, compression, image_size = struct.unpack_from('<IiiHHII', data, 14)
    stride = (width * 3 + 3) & ~3
    expected = stride * height
    if (size != len(data) or reserved or dib != 40 or bmp_width != width or bmp_height != -height or
            planes != 1 or bpp != 24 or compression != 0 or offset < 54 or
            offset + expected != len(data) or image_size not in (0, expected)):
        raise InvalidExport('invalid BMP format or dimensions')


def make_handler(directory):
    class Handler(BaseHTTPRequestHandler):
        def _send(self, code, body, content_type):
            self.send_response(code)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.end_headers()
            self.wfile.write(body)

        def _status(self):
            try:
                raw, mtime = _read_leaf(directory, 'status.json', MAX_JSON)
                obj = parse_status(raw)
                obj['age_seconds'] = round(max(0.0, time.time() - mtime), 3)
                obj['stale'] = obj['state'] == 'frame' and obj['age_seconds'] > 5
                return obj, raw
            except FileNotFoundError:
                return {'state': 'waiting', 'reason': 'Waiting for first capture', 'serial': '0', 'frame': '0',
                        'source_epoch': '0', 'source_ticket': 0, 'objects': 0, 'width': 0, 'height': 0,
                        'age_seconds': None, 'stale': False}, None

        def do_GET(self):
            url = urlsplit(self.path)
            if url.path == '/' and not url.query:
                return self._send(200, HTML, 'text/html; charset=utf-8')
            if url.path == '/favicon.ico' and not url.query:
                return self._send(204, b'', 'image/x-icon')
            if url.path == '/status.json' and not url.query:
                try:
                    obj, _ = self._status()
                except (InvalidExport, OSError):
                    obj = {'state': 'unavailable', 'reason': 'Invalid or unreadable export status',
                           'serial': '0', 'frame': '0', 'source_epoch': '0', 'source_ticket': 0,
                           'objects': 0, 'width': 0, 'height': 0, 'age_seconds': None, 'stale': False}
                return self._send(200, json.dumps(obj).encode('ascii'), 'application/json; charset=utf-8')
            if url.path != '/frame.bmp' or not re.fullmatch(r'serial=(0|[1-9][0-9]*)', url.query):
                return self._send(404, b'Not found', 'text/plain; charset=utf-8')
            requested = url.query[7:]
            try:
                obj, raw = self._status()
                if obj['state'] != 'frame' or obj['stale'] or requested != obj['serial']:
                    return self._send(409, b'Capture changed', 'text/plain; charset=utf-8')
                data, _ = _read_leaf(directory, 'frame-' + obj['slot'] + '.bmp', MAX_BMP)
                validate_bmp(data, obj['width'], obj['height'])
                try:
                    again, _ = _read_leaf(directory, 'status.json', MAX_JSON)
                except (InvalidExport, OSError):
                    return self._send(409, b'Capture changed', 'text/plain; charset=utf-8')
                if again != raw:
                    return self._send(409, b'Capture changed', 'text/plain; charset=utf-8')
                return self._send(200, data, 'image/bmp')
            except (InvalidExport, OSError):
                return self._send(422, b'Invalid capture', 'text/plain; charset=utf-8')

    return Handler


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', required=True, type=Path, help='exact snapshot export directory')
    parser.add_argument('--port', type=int, default=18711)
    args = parser.parse_args(argv)
    directory = args.directory.absolute()
    if not directory.is_dir() or directory.is_symlink():
        parser.error('export directory must be an existing, non-symlink directory')
    if not 1 <= args.port <= 65535:
        parser.error('port must be 1..65535')
    with ThreadingHTTPServer(('127.0.0.1', args.port), make_handler(directory)) as server:
        print(f'Native11 preview: http://127.0.0.1:{args.port}/', flush=True)
        server.serve_forever()


if __name__ == '__main__':
    main()
