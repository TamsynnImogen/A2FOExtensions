# A1 Classic Premonitions loader investigation — 2026-09-25

## Evidence and deployment

Reported failure: A1 Classic, Premonitions (`fed1.bzn`), after enabling
`A2FOMissions` and replacing the mod's MMM loader with the compatibility shim.
The active installation is `/home/tamsynn/NVMeData/Fleet Ops Roots/Data`.

The installed runtime (20:44) and mod-local loader (20:40) differed from the
current build (21:10 and 21:05 respectively). Disassembly of the installed
runtime confirmed that `script_extend_interface` still invoked `SetupReload`
and constructed `Worker` immediately; that constructor still patched save
buttons and walked the native entity table to stop nebula rotation. The current
source/build already defers these operations and constructs Worker on first
input instead. This session deployed those existing changes; it did not
establish the exact faulting instruction in the earlier run.

The 20:56 extension log confirms the module loaded and the selector launched
`fed1.bzn`. Its last messages concern nebula mesh preparation. The old module
does not contain the newer handoff milestone messages, so their absence does
not establish that the shim was never called. The installation's `bugreport.txt`
dates from August and is unrelated.

After confirming Armada was closed, backed up both DLLs and the previous log to:

`Data/rollback/missions-premonitions-20260925-212404/`

`manifest.json` records original and replacement hashes and full paths. Both
backups were verified before replacing the active files, and installed hashes
were verified against the build:

| File relative to Data | Installed SHA-256 |
| --- | --- |
| `modules/A2FOMissions.dll` | `ac76759f949d3598473c44e1d12813492850e1fd0522470760f7fdae0132c24e` |
| `Mods/STA1 Classic Mod/missions/mmm_loader.dll` | `1185ab97ac4f3115fdfd8d9ef845d4ef34766106142c3ed8cc0f2578818736fc` |

The parent `Data/missions/mmm_loader.dll` was not changed.

## Checks

`make missions` passed. A new headless integration probe passed with the current
runtime and shim. It checks export resolution, module initialization, null and
pre-attach calls, version/status returns, the native-style callback registration
through ScriptInterface slot 266, and shim detach/reload before Worker creation.

```sh
cd /home/tamsynn/A2FOHookExtensions
i686-w64-mingw32-g++ -std=gnu++17 -Wall -Wextra -Werror -static \
  -Isdk/include tests/missions_handoff_smoke.cpp \
  -o build/missions_handoff_smoke.exe
env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all \
  wine build/missions_handoff_smoke.exe \
  build/modules/A2FOMissions.dll build/missions/mmm_loader.dll
```

The probe deliberately does not execute the input callback, Worker/Lua startup,
or native game functions. It does not prove Premonitions plays successfully.

## Remaining acceptance

Manual new-game load of A1 Classic → Premonitions is pending. Check
`Data/A2FOExtensions.log` for the successive ScriptExtendInterface, ScriptInit,
first input, Worker construction and Lua-startup milestones. If it fails,
preserve that log before starting another session and investigate the last
completed stage. Mission restart/cleanup and saved-game resume remain unverified;
the current source explicitly defers the original savegame reload bridge.

To roll back with Armada closed, copy the two DLLs from their matching relative
paths beneath the backup directory over the active files. Keep the manifest and
before-log for comparison.
