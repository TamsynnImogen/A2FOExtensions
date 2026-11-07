#!/usr/bin/env python3
"""Supplement MMM's trimmed Boost snapshot with the matching full Boost 1.43 headers.

Megadroid Mission Mod vendors Boost 1.43, but its repository contains only the
subset needed by the original Visual C++ build.  MinGW takes different Boost
preprocessor/smart-pointer branches and therefore needs headers that are absent
from that trimmed tree.  This tool downloads the official Boost 1.43.0 source
archive and copies *only missing* files into external/boost, preserving every
file already supplied by MMM.
"""

from __future__ import annotations

import hashlib
import io
import os
import shutil
import sys
import tarfile
import urllib.error
import urllib.request
from pathlib import Path, PurePosixPath

BOOST_VERSION = "1.43.0"
BOOST_ARCHIVE = "boost_1_43_0.tar.bz2"
BOOST_URL = f"https://archives.boost.io/release/{BOOST_VERSION}/source/{BOOST_ARCHIVE}"
BOOST_SHA256 = "344f100b1aa410e812cabf0e4130728a80be042bf346135516b9187853806120"
ARCHIVE_ROOT = PurePosixPath("boost_1_43_0/boost")
LOCAL_ARCHIVE_ENV = "A2FO_BOOST_143_ARCHIVE"

# These are representative headers that are missing from MMM's trimmed Boost
# tree and are reached by the MinGW build.  The supplement operation copies all
# missing Boost headers, not just this probe list.
REQUIRED_HEADERS = (
    "preprocessor/control/detail/while.hpp",
    "config/compiler/gcc.hpp",
    "config/stdlib/libstdcpp3.hpp",
    "smart_ptr/detail/sp_counted_base_gcc_x86.hpp",
    "smart_ptr/detail/spinlock_sync.hpp",
)

MARKER_NAME = ".a2fo-full-boost-1.43.0"


def boost_dir(source_root: Path) -> Path:
    return source_root / "external" / "boost"


def complete(source_root: Path) -> bool:
    root = boost_dir(source_root)
    return all((root / item).is_file() for item in REQUIRED_HEADERS)


def read_archive() -> bytes:
    local = os.environ.get(LOCAL_ARCHIVE_ENV)
    if local:
        path = Path(local).expanduser().resolve()
        if not path.is_file():
            raise RuntimeError(f"{LOCAL_ARCHIVE_ENV} does not point to a file: {path}")
        print(f"Using local Boost 1.43 archive: {path}")
        return path.read_bytes()

    request = urllib.request.Request(
        BOOST_URL,
        headers={"User-Agent": "A2FOExtensions-MMM-Boost-vendor/1.0"},
    )
    try:
        print(f"Downloading full Boost {BOOST_VERSION} headers...")
        with urllib.request.urlopen(request, timeout=120) as response:
            return response.read()
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(
            "Unable to download the official Boost 1.43.0 source archive.\n"
            f"URL: {BOOST_URL}\n"
            f"Reason: {exc}\n"
            "You can download it manually and retry with:\n"
            f"  {LOCAL_ARCHIVE_ENV}=/path/to/{BOOST_ARCHIVE} ./build.sh"
        ) from exc


def verify_archive(payload: bytes) -> None:
    digest = hashlib.sha256(payload).hexdigest()
    if digest != BOOST_SHA256:
        raise RuntimeError(
            "Boost archive SHA-256 mismatch. Refusing to use it.\n"
            f"Expected: {BOOST_SHA256}\n"
            f"Actual:   {digest}"
        )


def member_relative_path(name: str) -> PurePosixPath | None:
    path = PurePosixPath(name)
    parts = path.parts
    prefix = ARCHIVE_ROOT.parts
    if len(parts) <= len(prefix) or parts[: len(prefix)] != prefix:
        return None
    rel = PurePosixPath(*parts[len(prefix) :])
    if not rel.parts or any(part in ("", ".", "..") for part in rel.parts):
        return None
    return rel


def supplement(source_root: Path, payload: bytes) -> int:
    target_root = boost_dir(source_root)
    if not target_root.is_dir():
        raise RuntimeError(f"MMM Boost directory not found: {target_root}")

    copied = 0
    with tarfile.open(fileobj=io.BytesIO(payload), mode="r:bz2") as archive:
        for member in archive:
            if not member.isfile():
                continue
            rel = member_relative_path(member.name)
            if rel is None:
                continue
            target = target_root.joinpath(*rel.parts)
            if target.exists():
                continue
            source = archive.extractfile(member)
            if source is None:
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with source, target.open("wb") as dst:
                shutil.copyfileobj(source, dst)
            copied += 1

    if not complete(source_root):
        missing = [item for item in REQUIRED_HEADERS if not (target_root / item).is_file()]
        raise RuntimeError(
            "Boost 1.43 supplement completed but required headers are still missing:\n  "
            + "\n  ".join(missing)
        )

    marker = target_root / MARKER_NAME
    marker.write_text(
        "source=https://archives.boost.io/release/1.43.0/source/boost_1_43_0.tar.bz2\n"
        f"sha256={BOOST_SHA256}\n"
        "mode=copy-missing-only\n",
        encoding="utf-8",
    )
    return copied


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: vendor_mmm_boost.py <mmm-source-root>", file=sys.stderr)
        return 2

    source_root = Path(sys.argv[1]).resolve()
    if not (source_root / "external" / "boost" / "version.hpp").is_file():
        print(f"MMM Boost snapshot not found under {source_root}", file=sys.stderr)
        return 2

    if complete(source_root):
        print(f"MMM Boost 1.43 header set already complete under {boost_dir(source_root)}")
        return 0

    payload = read_archive()
    verify_archive(payload)
    copied = supplement(source_root, payload)
    print(
        f"Supplemented MMM Boost 1.43 with {copied} missing file(s) under "
        f"{boost_dir(source_root)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, tarfile.TarError) as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
