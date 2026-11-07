#!/usr/bin/env bash
set -euo pipefail

FFMPEG_VERSION="${FFMPEG_VERSION:-9.0.2}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${FFMPEG_PREFIX:-${ROOT}/third_party/ffmpeg-win32}"
WORK="${FFMPEG_WORK:-${ROOT}/.build/ffmpeg-win32}"
TARBALL="${WORK}/ffmpeg-${FFMPEG_VERSION}.tar.xz"
SOURCE="${WORK}/ffmpeg-${FFMPEG_VERSION}"
URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

for tool in i686-w64-mingw32-gcc i686-w64-mingw32-g++ make tar pkg-config; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Missing required tool: $tool" >&2
        exit 1
    }
done

mkdir -p "$WORK" "$PREFIX"

if [[ ! -f "$TARBALL" ]]; then
    if command -v curl >/dev/null 2>&1; then
        curl -fL "$URL" -o "$TARBALL"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$TARBALL" "$URL"
    else
        echo "Need curl or wget to fetch $URL" >&2
        exit 1
    fi
fi

if [[ ! -d "$SOURCE" ]]; then
    tar -xf "$TARBALL" -C "$WORK"
fi

cd "$SOURCE"
make distclean >/dev/null 2>&1 || true

./configure \
    --prefix="$PREFIX" \
    --target-os=mingw32 \
    --arch=x86 \
    --cpu=i686 \
    --cross-prefix=i686-w64-mingw32- \
    --enable-cross-compile \
    --enable-static \
    --disable-shared \
    --disable-programs \
    --disable-doc \
    --disable-debug \
    --disable-network \
    --disable-autodetect \
    --disable-iconv \
    --disable-zlib \
    --disable-bzlib \
    --disable-lzma \
    --disable-x86asm \
    --disable-everything \
    --enable-protocol=file \
    --enable-demuxer=mov,matroska \
    --enable-decoder=h264,hevc,mpeg4,vp8,vp9,av1,aac,opus,vorbis,mp3,theora,flac,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le \
    --enable-swscale \
    --enable-swresample

make -j"${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
make install

echo
printf 'Built static 32-bit Windows FFmpeg %s SDK at:\n  %s\n' "$FFMPEG_VERSION" "$PREFIX"
