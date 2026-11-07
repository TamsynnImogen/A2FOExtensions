#!/usr/bin/env python3
"""Vendor the pinned Megadroid Mission Mod source used by A2FOMissions.

The runtime assets already live in third_party/mmm.  This script downloads only
source/build dependencies required to compile A2FOMissions and places them in
third_party/mmm/source.  It deliberately avoids requiring git.
"""

from __future__ import annotations

import io
import shutil
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

COMMIT = "72d6858b22759613e6ce161a834bbd9c99ad7a5f"
URL = f"https://codeload.github.com/chreden/mmm/zip/{COMMIT}"
ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "third_party" / "mmm" / "source"
MARKER = DEST / ".a2fo-mmm-source"

REQUIRED_PREFIXES = (
    "mmm_armada/",
    "mmm_common/",
    "mmm_database/",
    "mmm_luapack/",
    "mmm_loader/",
    "external/boost/",
    "external/lua/",
    "external/luabind/",
    "external/zlib/",
)

REQUIRED_FILES = (
    "mmm_armada/mmm_armada.vcxproj",
    "mmm_common/mmm_common.vcxproj",
    "mmm_database/mmm_database.vcxproj",
    "mmm_luapack/mmm_luapack.vcxproj",
    "mmm_loader/Worker.cpp",
    "mmm_loader/RuleHelper.cpp",
    "external/luabind/LuaBind.vcxproj",
    "external/lua/lua.vcxproj",
    "external/zlib/zlib.h",
    "external/boost/smart_ptr/detail/sp_counted_base.hpp",
    "external/boost/smart_ptr/detail/sp_counted_base_w32.hpp",
    "external/boost/smart_ptr/detail/spinlock.hpp",
    "external/boost/smart_ptr/detail/spinlock_w32.hpp",
)


def complete(path: Path) -> bool:
    return all((path / item).is_file() for item in REQUIRED_FILES)


def download() -> bytes:
    request = urllib.request.Request(
        URL,
        headers={"User-Agent": "A2FOExtensions-MMM-vendor/1.0"},
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.read()
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(
            "Unable to download the pinned MMM source. Check the internet "
            f"connection and retry, or manually place commit {COMMIT} under "
            f"{DEST}.\nURL: {URL}\nReason: {exc}"
        ) from exc


def safe_member_path(name: str) -> tuple[str, ...] | None:
    parts = PurePosixPath(name).parts
    if len(parts) < 2:
        return None
    rel = parts[1:]
    if any(part in ("", ".", "..") for part in rel):
        return None
    rel_text = "/".join(rel)
    if not any(rel_text.startswith(prefix) for prefix in REQUIRED_PREFIXES):
        return None
    return rel


def main() -> int:
    if complete(DEST):
        print(f"MMM source already present: {DEST}")
        return 0

    if DEST.exists() and any(DEST.iterdir()):
        print(
            f"Refusing to overwrite incomplete/non-empty MMM source tree: {DEST}\n"
            "Move or remove that directory, then rerun make vendor-mmm-source.",
            file=sys.stderr,
        )
        return 2

    print(f"Downloading MMM source commit {COMMIT}...")
    payload = download()

    temp_parent = DEST.parent
    temp_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mmm-source-", dir=temp_parent) as temp_name:
        temp = Path(temp_name)
        with zipfile.ZipFile(io.BytesIO(payload)) as archive:
            for info in archive.infolist():
                if info.is_dir():
                    continue
                rel = safe_member_path(info.filename)
                if rel is None:
                    continue
                target = temp.joinpath(*rel)
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as src, target.open("wb") as dst:
                    shutil.copyfileobj(src, dst)

        if not complete(temp):
            missing = [item for item in REQUIRED_FILES if not (temp / item).is_file()]
            print("Downloaded MMM archive is missing required files:", file=sys.stderr)
            for item in missing:
                print(f"  {item}", file=sys.stderr)
            return 3

        (temp / ".a2fo-mmm-source").write_text(
            f"upstream=https://github.com/chreden/mmm\ncommit={COMMIT}\n",
            encoding="utf-8",
        )
        if DEST.exists():
            DEST.rmdir()
        temp.rename(DEST)

    print(f"Vendored MMM source to {DEST}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
