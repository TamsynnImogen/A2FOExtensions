# A2FOMissions

`A2FOMissions.dll` hosts the retained MMM framework for A2FOExtensions. Rust now
owns the mission handoff controller and the compatibility loader DLL.  Armada still loads
`missions/mmm_loader.dll`, but that DLL is now only a compatibility shim which
forwards `ScriptExtendInterface` and `ScriptInit` into this module.

The MMM source may be stored either directly beneath `third_party/mmm/` (with
`mmm_armada`, `mmm_common`, etc.) or beneath `third_party/mmm/source/`.  The
Makefile detects both layouts.  `make vendor-mmm-source` populates the latter
from the pinned upstream commit when the source is not already vendored.

The original `mmm_loader/Main.cpp` is intentionally not linked.  It owns a
global `Worker` whose constructor runs at DLL load time.  A2FOMissions instead
uses the Rust handoff controller to create `Worker` lazily on the first mission
input callback, after Armada
attaches its ScriptInterface and calls ScriptInit for a new game.

`mmm_loader/Worker.cpp` and `RuleHelper.cpp`, plus the MMM libraries and their
Lua/Luabind/zlib dependencies, are compiled directly into A2FOMissions.dll.

## MinGW note

MMM's bundled Boost snapshot contains only the Visual C++ compiler and
Dinkumware standard-library config headers.  The Makefile therefore disables
that old Boost autodetection when building with MinGW while retaining the
bundled header implementation.  `MMM_BOOST_CPPFLAGS` can be overridden with an
earlier `-I...` path if a newer Boost tree is preferred.

## Source compatibility patch

`make patch-mmm-source` applies the currently required MinGW source edits:
MMM's `mmm_luapack/Node.h` uses MSVC-only `__declspec(novtable)`, and
`mmm_armada/Race_Internal.cpp` uses MSVC's two-argument `strcpy_s` template
overload.  The patch keeps the MSVC behaviour where appropriate and uses a
bounded `std::strncpy` fallback under the cross build.  It is idempotent and
refuses to guess if upstream changes the expected source.  All MMM object rules
depend on this target.

If `third_party/mmm/source` is absent, run `make vendor-mmm-source` once and
then rerun `./build.sh`.  The second make invocation is intentional: the MMM
Visual Studio project files are read while GNU make parses the Makefile.

## Runtime validation status

The current handoff defers the legacy reload-rule bridge. The retained native
Worker still calls its legacy save-button and nebula-rotation helpers during
construction and its save-button restoration helper during destruction; these
operations are unchanged by the Rust handoff migration and need engine-specific
runtime validation. Savegame resume is not validated:
the original reload bridge is what re-registers the input callback on resume.
First-input Lua startup still uses native MMM bindings and needs an in-game
test; a successful DLL load or handoff smoke test does not validate those calls.

Build both components with `make missions`. Install
`build/modules/A2FOMissions.dll` into the active extension root's `modules/`
directory and `build/missions/mmm_loader.dll` into the selected mod's
`missions/` directory. Back up both existing files and compare SHA-256 hashes
after copying. A mod-local loader takes precedence over the parent's loader.

For crash diagnosis, inspect `A2FOExtensions.log` for these milestones:

1. `ScriptExtendInterface entered` and `MMM ScriptInterface attached`.
2. `ScriptInit entered` and `MMM input callback registered`.
3. `first MMM input update entered` and `MMM Worker construction completed`.
4. `starting MMM new-game Lua runtime` and `MMM new-game Lua runtime started`.

Older installed builds do not emit all these milestones. Check deployed hashes
before interpreting their absence. See [A1 Classic deployment investigation](../../docs/missions-premonitions-investigation.md).

## Rust replacement status

See [MMM migration](../../docs/rust-mmm-migration.md) for the exact migrated
scope, differential tests and remaining VM/framework work. `make missions` builds
the Rust loader through `tools/build_rust_mmm_loader.py`; the C++ shim source
remains a test reference. The complete mission framework is still a native
dependency and no in-game acceptance is implied.
