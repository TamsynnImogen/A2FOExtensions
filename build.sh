#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "Missing i686-w64-mingw32-g++." >&2
    echo "On Nobara/Fedora install it with:" >&2
    echo "  sudo dnf install mingw32-gcc-c++ mingw32-binutils make" >&2
    exit 1
fi

make clean
make -j"$(nproc)"
make verify

echo
echo "Build complete. Output is in: $(pwd)/build"
