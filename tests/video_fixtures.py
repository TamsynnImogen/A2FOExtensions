"""Create synthetic decoder fixtures; optionally use a supplied final movie."""
import argparse
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--output', default='build/video-fixtures')
parser.add_argument('--reference-movie')
args = parser.parse_args()
out = Path(args.output)
out.mkdir(parents=True, exist_ok=True)
fixtures = [('aac.mp4', 'aac', '0.35'), ('short.mp4', 'aac', '0.06'),
            ('opus.webm', 'libopus', '0.35'), ('vorbis.webm', 'libvorbis', '0.35'),
            ('silent.mp4', None, '0.35')]
if not args.reference_movie:
    fixtures.append(('reference.mp4', 'aac', '3.0'))
for name, codec, duration in fixtures:
    command = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-nostdin',
               '-f', 'lavfi', '-i', 'color=c=blue:s=32x32:r=25']
    if codec:
        command += ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=22050']
    command += ['-t', duration, '-c:v', 'libvpx-vp9' if name.endswith('webm') else 'libx264',
                '-threads', '1', '-pix_fmt', 'yuv420p']
    if codec:
        command += ['-c:a', codec]
    subprocess.run(command + ['-y', str(out / name)], check=True)
if args.reference_movie:
    source = Path(args.reference_movie)
    if source.resolve() != (out / 'reference.mp4').resolve():
        shutil.copyfile(source, out / 'reference.mp4')
