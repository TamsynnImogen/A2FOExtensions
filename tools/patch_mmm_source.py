#!/usr/bin/env python3
"""Apply small, documented MinGW compatibility edits to vendored MMM source."""

from __future__ import annotations

import sys
from pathlib import Path

NOVTABLE_ORIGINAL = "class __declspec(novtable) Node"
NOVTABLE_PATCHED = """#if defined(_MSC_VER)\n#  define MMM_NOVTABLE __declspec(novtable)\n#else\n#  define MMM_NOVTABLE\n#endif\n\t\tclass MMM_NOVTABLE Node"""

RACE_MUSIC_ORIGINAL = "strcpy_s( race_->m_singlePlayerMusic, music.c_str() );"
RACE_MUSIC_PATCHED = """std::strncpy(race_->m_singlePlayerMusic, music.c_str(),\n\t\t\tsizeof(race_->m_singlePlayerMusic) - 1);\n\t\trace_->m_singlePlayerMusic[sizeof(race_->m_singlePlayerMusic) - 1] = '\\0';"""

RACE_DISPLAY_ORIGINAL = "strcpy_s( race_->m_displayKey, name.c_str() );"
RACE_DISPLAY_PATCHED = """std::strncpy(race_->m_displayKey, name.c_str(),\n\t\t\tsizeof(race_->m_displayKey) - 1);\n\t\trace_->m_displayKey[sizeof(race_->m_displayKey) - 1] = '\\0';"""

BOOST_COUNTED_BASE_ORIGINAL = """#elif defined( __GNUC__ ) && ( defined( __i386__ ) || defined( __x86_64__ ) )
# include <boost/smart_ptr/detail/sp_counted_base_gcc_x86.hpp>"""
BOOST_COUNTED_BASE_PATCHED = """#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)) && defined(__GNUC__)
// A2FO/MinGW: MMM vendors only the Win32 counted-base backend.
# include <boost/smart_ptr/detail/sp_counted_base_w32.hpp>

#elif defined( __GNUC__ ) && ( defined( __i386__ ) || defined( __x86_64__ ) )
# include <boost/smart_ptr/detail/sp_counted_base_gcc_x86.hpp>"""

BOOST_SPINLOCK_ORIGINAL = """#elif defined( BOOST_SP_HAS_SYNC )
#  include <boost/smart_ptr/detail/spinlock_sync.hpp>

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#  include <boost/smart_ptr/detail/spinlock_w32.hpp>"""

LUAPACK_INCLUDE_ORIGINAL = '#include "../mmm_luapack/Luapack.h"'
LUAPACK_INCLUDE_PATCHED = '#include "../mmm_luapack/LuaPack.h"'

ST3D_DEVICE_TYPE_ORIGINAL = "enum ST3D_DeviceType m_platform;"
ST3D_DEVICE_TYPE_PATCHED = "int m_platform; // A2FO/MinGW: opaque ST3D_DeviceType storage (32-bit)"

ST3D_PIXEL_FORMAT_ORIGINAL = "enum ST3D_PixelFormat m_pf;"
ST3D_PIXEL_FORMAT_PATCHED = "int m_pf; // A2FO/MinGW: opaque ST3D_PixelFormat storage (32-bit)"

ST3D_INTERPOLATION_ORIGINAL = "enum ST3D_Interpolation m_interpolation;"
ST3D_INTERPOLATION_PATCHED = "int m_interpolation; // A2FO/MinGW: opaque ST3D_Interpolation storage (32-bit)"

ARMADA_EXTERNALS_WINSOCK_ORIGINAL = """#include <process.h>
#include <windows.h>"""
ARMADA_EXTERNALS_WINSOCK_PATCHED = """#include <process.h>

// A2FO/MinGW: WIN32_LEAN_AND_MEAN prevents windows.h from supplying Winsock.
// MMM's DebuggerConnection uses Winsock directly, so include Winsock 2 first.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>"""

TYPE_ENTITY_INCLUDE_ORIGINAL = '#include "type_entity.h"'
TYPE_ENTITY_INCLUDE_PATCHED = '#include "Type_Entity.h"'

DEBRIEFING_CSTDINT_ORIGINAL = '#include "ListAdapter.h"'
DEBRIEFING_CSTDINT_PATCHED = '#include "ListAdapter.h"\n\n#include <cstdint>'

COMMON_WINDOWS_INCLUDE_ORIGINAL = '#include <Windows.h>'
COMMON_WINDOWS_INCLUDE_PATCHED = '#include <windows.h>'

COMMON_SHLOBJ_INCLUDE_ORIGINAL = '#include <Shlobj.h>'
COMMON_SHLOBJ_INCLUDE_PATCHED = '#include <shlobj.h>'

# A2FO/MinGW: MMM's MSVC project is Unicode, but our MinGW build does not
# globally define UNICODE. Keep this translation unit explicit rather than
# changing the character-set assumptions of the entire vendored codebase.
COMMON_GETMODULE_ORIGINAL = 'GetModuleHandle(L"FleetOpsHook.dll")'
COMMON_GETMODULE_PATCHED = 'GetModuleHandleW(L"FleetOpsHook.dll")'
COMMON_MODULE_CHECK_ORIGINAL = 'fleetops = mod != INVALID_HANDLE_VALUE;'
COMMON_MODULE_CHECK_PATCHED = 'fleetops = mod != nullptr;'
COMMON_GETFILENAME_ORIGINAL = 'GetModuleFileName(mod, &path[0], MAX_PATH)'
COMMON_GETFILENAME_PATCHED = 'GetModuleFileNameW(mod, &path[0], MAX_PATH)'
COMMON_VERSION_SIZE_ORIGINAL = 'GetFileVersionInfoSize(path, nullptr)'
COMMON_VERSION_SIZE_PATCHED = 'GetFileVersionInfoSizeW(path, nullptr)'
COMMON_VERSION_INFO_ORIGINAL = 'GetFileVersionInfo(path, 0, size, &version_info_data[0]);'
COMMON_VERSION_INFO_PATCHED = 'GetFileVersionInfoW(path, 0, size, &version_info_data[0]);'
COMMON_VERSION_UINT_ORIGINAL = 'uint32_t version_info_size = sizeof(VS_FIXEDFILEINFO);'
COMMON_VERSION_UINT_PATCHED = 'UINT version_info_size = sizeof(VS_FIXEDFILEINFO);'
COMMON_VERQUERY_ORIGINAL = r'VerQueryValue(&version_info_data[0], L"\\", reinterpret_cast<LPVOID*>(&version_info), &version_info_size);'
COMMON_VERQUERY_PATCHED = r'VerQueryValueW(&version_info_data[0], L"\\", reinterpret_cast<LPVOID*>(&version_info), &version_info_size);'

LUAPACK_CSTRING_ORIGINAL = '#include <regex>\n#include <iostream>'
LUAPACK_CSTRING_PATCHED = '#include <regex>\n#include <iostream>\n#include <cstring>'

WORKER_CSTDINT_ORIGINAL = '#include "../mmm_armada/Type_Nebula.h"'
WORKER_CSTDINT_PATCHED = '#include "../mmm_armada/Type_Nebula.h"\n\n#include <cstdint>'

BOOST_SPINLOCK_PATCHED = """#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)) && defined(__GNUC__)
// A2FO/MinGW: MMM vendors only the Win32 spinlock backend.
#  include <boost/smart_ptr/detail/spinlock_w32.hpp>

#elif defined( BOOST_SP_HAS_SYNC )
#  include <boost/smart_ptr/detail/spinlock_sync.hpp>

#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#  include <boost/smart_ptr/detail/spinlock_w32.hpp>"""

MSVC_RTTI_HEADER = r"""#pragma once

#include <cstdint>
#include <cstring>

// A2FO/MinGW compatibility for MMM's original use of MSVC type_info::raw_name().
// Armada/Fleet Operations objects carry MSVC x86 RTTI. MinGW's C++ RTTI ABI is
// different, so typeid(...).name() is not a valid replacement. Read the decorated
// class name directly from the MSVC vftable RTTI Complete Object Locator instead.
namespace mmm
{
namespace a2fo_compat
{
#pragma pack(push, 4)
struct MsvcTypeDescriptor32
{
    std::uint32_t vftable;
    std::uint32_t spare;
    char name[1];
};

struct MsvcCompleteObjectLocator32
{
    std::uint32_t signature;
    std::uint32_t offset;
    std::uint32_t cd_offset;
    std::uint32_t type_descriptor;
    std::uint32_t class_descriptor;
};
#pragma pack(pop)

inline const char* msvc_raw_type_name(const void* object) noexcept
{
    static_assert(sizeof(void*) == 4,
                  "MMM MSVC RTTI compatibility requires a 32-bit target");

    if (!object)
        return nullptr;

    const auto vtable = *reinterpret_cast<const std::uintptr_t* const*>(object);
    if (!vtable)
        return nullptr;

    // MSVC x86 stores a pointer to RTTICompleteObjectLocator immediately before
    // the first virtual-function slot. Its TypeDescriptor pointer is absolute.
    const auto locator = reinterpret_cast<const MsvcCompleteObjectLocator32*>(vtable[-1]);
    if (!locator || locator->type_descriptor == 0)
        return nullptr;

    const auto descriptor = reinterpret_cast<const MsvcTypeDescriptor32*>(
        static_cast<std::uintptr_t>(locator->type_descriptor));
    return descriptor ? descriptor->name : nullptr;
}

inline bool msvc_raw_type_is(const void* object, const char* expected) noexcept
{
    const char* actual = msvc_raw_type_name(object);
    return actual && expected && std::strcmp(actual, expected) == 0;
}
}
}
"""


def replace_once(path: Path, original: str, replacement: str, marker: str) -> bool:
    text = path.read_text(encoding="utf-8-sig")
    if marker in text:
        return False
    if original not in text:
        raise RuntimeError(
            f"Cannot apply expected MMM compatibility patch to {path}:\n"
            f"  expected text not found: {original}"
        )
    path.write_text(text.replace(original, replacement, 1), encoding="utf-8")
    return True


def patch_msvc_raw_name(path: Path, pointer_name: str) -> bool:
    import re

    text = path.read_text(encoding="utf-8-sig")
    changed = False

    include_line = '#include "A2FO_MsvcRtti.h"'
    if include_line not in text:
        lines = text.splitlines()
        lines.insert(1 if lines else 0, include_line)
        text = "\n".join(lines) + ("\n" if text.endswith("\n") else "")
        changed = True

    pattern = re.compile(
        rf'std::string\(typeid\(\*{re.escape(pointer_name)}\)\.raw_name\(\)\)\s*==\s*"([^"]+)"'
    )
    text, count = pattern.subn(
        lambda match: f'a2fo_compat::msvc_raw_type_is({pointer_name}, "{match.group(1)}")',
        text,
    )
    if count:
        changed = True

    if "raw_name()" in text:
        raise RuntimeError(f"Unpatched MSVC type_info::raw_name() use remains in {path}")

    if changed:
        path.write_text(text, encoding="utf-8")
    return changed


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: patch_mmm_source.py <mmm-source-root>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    node = root / "mmm_luapack" / "Node.h"
    common_externals = root / "mmm_common" / "Externals.h"
    race = root / "mmm_armada" / "Race_Internal.cpp"
    counted_base = root / "external" / "boost" / "smart_ptr" / "detail" / "sp_counted_base.hpp"
    spinlock = root / "external" / "boost" / "smart_ptr" / "detail" / "spinlock.hpp"
    gfx_config = root / "mmm_armada" / "Type_ST3D_GraphicsEngine_Configuration.h"
    display_mode = root / "mmm_armada" / "Type_ST3D_DisplayMode.h"
    animation_channel = root / "mmm_armada" / "Type_ST3D_AnimationChannel.h"
    armada_externals = root / "mmm_armada" / "Externals.h"
    type_entity = root / "mmm_armada" / "Type_Entity.cpp"
    type_gameobject_class = root / "mmm_armada" / "Type_GameObjectClass.cpp"
    debriefing = root / "mmm_armada" / "Type_DebriefingData.h"
    common_cpp = root / "mmm_common" / "mmm_common.cpp"
    mod_folder = root / "mmm_common" / "ModFolder.cpp"
    luapack_pack = root / "mmm_luapack" / "Pack.cpp"
    worker_cpp = root / "mmm_loader" / "Worker.cpp"
    rtti_header = root / "mmm_armada" / "A2FO_MsvcRtti.h"
    for path in (
        node, common_externals, race, counted_base, spinlock,
        gfx_config, display_mode, animation_channel, armada_externals,
        type_entity, type_gameobject_class, debriefing, common_cpp, mod_folder, luapack_pack, worker_cpp,
    ):
        if not path.is_file():
            print(f"MMM source file missing: {path}", file=sys.stderr)
            return 2

    changes = 0
    changes += replace_once(node, NOVTABLE_ORIGINAL, NOVTABLE_PATCHED, "MMM_NOVTABLE")
    changes += replace_once(
        common_externals,
        LUAPACK_INCLUDE_ORIGINAL,
        LUAPACK_INCLUDE_PATCHED,
        LUAPACK_INCLUDE_PATCHED,
    )
    changes += replace_once(
        counted_base,
        BOOST_COUNTED_BASE_ORIGINAL,
        BOOST_COUNTED_BASE_PATCHED,
        "A2FO/MinGW: MMM vendors only the Win32 counted-base backend",
    )
    changes += replace_once(
        spinlock,
        BOOST_SPINLOCK_ORIGINAL,
        BOOST_SPINLOCK_PATCHED,
        "A2FO/MinGW: MMM vendors only the Win32 spinlock backend",
    )
    changes += replace_once(
        gfx_config,
        ST3D_DEVICE_TYPE_ORIGINAL,
        ST3D_DEVICE_TYPE_PATCHED,
        "A2FO/MinGW: opaque ST3D_DeviceType storage",
    )
    changes += replace_once(
        display_mode,
        ST3D_PIXEL_FORMAT_ORIGINAL,
        ST3D_PIXEL_FORMAT_PATCHED,
        "A2FO/MinGW: opaque ST3D_PixelFormat storage",
    )
    changes += replace_once(
        animation_channel,
        ST3D_INTERPOLATION_ORIGINAL,
        ST3D_INTERPOLATION_PATCHED,
        "A2FO/MinGW: opaque ST3D_Interpolation storage",
    )
    changes += replace_once(
        armada_externals,
        ARMADA_EXTERNALS_WINSOCK_ORIGINAL,
        ARMADA_EXTERNALS_WINSOCK_PATCHED,
        "A2FO/MinGW: WIN32_LEAN_AND_MEAN prevents windows.h from supplying Winsock",
    )

    changes += replace_once(
        type_entity,
        TYPE_ENTITY_INCLUDE_ORIGINAL,
        TYPE_ENTITY_INCLUDE_PATCHED,
        TYPE_ENTITY_INCLUDE_PATCHED,
    )
    changes += replace_once(
        debriefing,
        DEBRIEFING_CSTDINT_ORIGINAL,
        DEBRIEFING_CSTDINT_PATCHED,
        "#include <cstdint>",
    )

    changes += replace_once(
        common_cpp,
        COMMON_WINDOWS_INCLUDE_ORIGINAL,
        COMMON_WINDOWS_INCLUDE_PATCHED,
        COMMON_WINDOWS_INCLUDE_PATCHED,
    )
    changes += replace_once(
        mod_folder,
        COMMON_SHLOBJ_INCLUDE_ORIGINAL,
        COMMON_SHLOBJ_INCLUDE_PATCHED,
        COMMON_SHLOBJ_INCLUDE_PATCHED,
    )

    # The original Visual Studio project builds as Unicode.  Use the explicit
    # wide Win32 APIs here so this remains correct under our non-UNICODE MinGW
    # build, and fix GetModuleHandle's documented NULL failure result.
    changes += replace_once(
        common_cpp, COMMON_GETMODULE_ORIGINAL, COMMON_GETMODULE_PATCHED, COMMON_GETMODULE_PATCHED
    )
    changes += replace_once(
        common_cpp, COMMON_MODULE_CHECK_ORIGINAL, COMMON_MODULE_CHECK_PATCHED, COMMON_MODULE_CHECK_PATCHED
    )
    changes += replace_once(
        common_cpp, COMMON_GETFILENAME_ORIGINAL, COMMON_GETFILENAME_PATCHED, COMMON_GETFILENAME_PATCHED
    )
    changes += replace_once(
        common_cpp, COMMON_VERSION_SIZE_ORIGINAL, COMMON_VERSION_SIZE_PATCHED, COMMON_VERSION_SIZE_PATCHED
    )
    changes += replace_once(
        common_cpp, COMMON_VERSION_INFO_ORIGINAL, COMMON_VERSION_INFO_PATCHED, COMMON_VERSION_INFO_PATCHED
    )
    changes += replace_once(
        common_cpp, COMMON_VERSION_UINT_ORIGINAL, COMMON_VERSION_UINT_PATCHED, COMMON_VERSION_UINT_PATCHED
    )
    changes += replace_once(
        common_cpp, COMMON_VERQUERY_ORIGINAL, COMMON_VERQUERY_PATCHED, COMMON_VERQUERY_PATCHED
    )
    changes += replace_once(
        luapack_pack, LUAPACK_CSTRING_ORIGINAL, LUAPACK_CSTRING_PATCHED, '#include <cstring>'
    )
    changes += replace_once(
        worker_cpp, WORKER_CSTDINT_ORIGINAL, WORKER_CSTDINT_PATCHED, '#include <cstdint>'
    )

    if not rtti_header.is_file() or rtti_header.read_text(encoding="utf-8-sig") != MSVC_RTTI_HEADER:
        rtti_header.write_text(MSVC_RTTI_HEADER, encoding="utf-8")
        changes += 1

    changes += patch_msvc_raw_name(type_entity, "ent")
    changes += patch_msvc_raw_name(type_gameobject_class, "ptr")

    race_text = race.read_text(encoding="utf-8-sig")
    if "std::strncpy" not in race_text:
        if "#include <cstring>" not in race_text:
            if '#include "Race_Internal.h"' not in race_text:
                raise RuntimeError(f"Unexpected Race_Internal.cpp include layout: {race}")
            race_text = race_text.replace(
                '#include "Race_Internal.h"',
                '#include "Race_Internal.h"\n\n#include <cstring>',
                1,
            )
        if RACE_MUSIC_ORIGINAL not in race_text or RACE_DISPLAY_ORIGINAL not in race_text:
            raise RuntimeError(
                f"Cannot apply expected strcpy_s compatibility patch to {race}"
            )
        race_text = race_text.replace(RACE_MUSIC_ORIGINAL, RACE_MUSIC_PATCHED, 1)
        race_text = race_text.replace(RACE_DISPLAY_ORIGINAL, RACE_DISPLAY_PATCHED, 1)
        race.write_text(race_text, encoding="utf-8")
        changes += 1

    if changes:
        print(f"Applied {changes} MMM MinGW compatibility patch(es) under {root}")
    else:
        print(f"MMM MinGW compatibility patches already applied under {root}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
