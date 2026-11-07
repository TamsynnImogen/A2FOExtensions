# Refit scene provider build and console check

`tools/build_refit_scene.sh` builds the vendored `menu-renderer` crate with rustup
toolchain `1.96.0` (override with `REFIT_RUST_TOOLCHAIN`), offline for Win32 MSVC.
It discovers `rust-lld` from that toolchain's sysroot. Output is
`build/modules/refit/A2FORefitScene.dll`. Config-only files are staged under
`build/refit-scene-resources`: `Render3D.ini`, `ordnance.ini`, and available
`*.ini` files from `menus`, `skies`, `hulls`, `surfaces`, and `nebulae`.
The source `menus` folder is staged as `Menu` to match the provider's
`Menu\\map.ini` lookup, so the staged map can override the built-in map.
The vendored tree currently has no `hulls` config directory. No artwork is copied.

Build and compile the Win32 console fixture from the repository root:

```sh
tools/build_refit_scene.sh
i686-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -static -static-libgcc -static-libstdc++ \
  -o build/refit_scene_provider.exe tests/refit_scene_provider.cpp
```

With an already initialized disposable Wine prefix, run the fixture without a
display. Pass 1 to 32 existing Fleet Ops asset roots in precedence order,
lowest to highest. The resource argument is the staged config root and receives
`A2FORefitScene.log`.

```sh
env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all \
  WINEPREFIX=/home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  WINEDLLOVERRIDES=winemenubuilder.exe=d wine \
  build/refit_scene_provider.exe build/modules/refit/A2FORefitScene.dll \
  build/refit-scene-resources '/home/tamsynn/NVMeData/Fleet Ops Roots/Data' \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STA2 Classic Mod' \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/test' \
  > build/refit-scene-provider-20261018.log 2>&1
```

The fixture loads only the provider DLL. It checks API version 1, rejected null
and malformed initialization, successful initialization with a null device,
zero draw result without a device, owner-thread `SetDevice(nullptr)`,
foreign-thread SetDevice/MenuScene/MenuWarp rejection, rejected
foreign-thread `SetDevice(nullptr)`, and rejected
repeat initialization. It creates no window or D3D device and never maps or runs
`ArmadaL.exe`. The provider log may show asset indexing and the `Menu` config
override; that is not a rendered scene, native client, or in-game acceptance test.
The 2026-10-18 three-root check passed and its provider log reported
`Menu\\map.ini in the game directory` after indexing 1,027 SODs, 3,983 textures,
64 sprite files, and 2,731 ODFs.

The staged host config now comes from `menu-renderer/a2fo-menu.ini`, a separate
menu performance profile with MSAA/bloom disabled and smaller bake targets.
The 19 October Data-only ABI fixture also passes; its log resolves all 16 menu
ship entries against the active Data root, including explicit Fleet Ops
fallbacks. This fixture still creates no graphics device and proves no GPU draw.
The script also stages Refit and libm licenses under build/modules/refit/licenses.
The separate installed-image probe source is tools/probe_refit_scene_images.rs.
It can be linked against an offline native release build of a2fmt, then invoked
with the three installed Textures directories; its failures include intentional
unsupported-format and size limits and are documented in the live scene record.
