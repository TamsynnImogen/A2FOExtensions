# A2FO Hook Extensions

`A2FOExtensions.dll` is a companion DLL for the Fleet Operations 4.0 build of
Star Trek: Armada II. It does not replace or modify `FleetOpsHook.dll`.

It adds two features:

- Recursive ODF discovery. ODF files in arbitrary subdirectories below `odf`
  are registered with Fleet Operations' own virtual filesystem, including ODF
  directories found inside active `odf.fpq` archives. At ParameterDB load time,
  the winning recursive file entry is selected by basename.
- A per-evolver `cocoon` command. The value is the cocoon SOD filename; `.sod`
  is optional.

Example:

```ini
classLabel = "evolver"
cocoon = "my_custom_cocoon.sod"
```

If `cocoon` is absent or empty, Fleet Operations keeps its existing behavior:
`8472_cocoon.sod`, or `8472_cocoon2.sod` when its existing
`crewHitPercent = 0` compatibility rule selects that model.

## Supported build

The hook deliberately refuses to patch unknown binaries. The initial release
supports this exact pair:

- `ArmadaL.exe`: timestamp `0x3c4c76bd`, image size `0x403999`
- `FleetOpsHook.dll`: timestamp `0x51f6475c`, image size `0x322000`

The tested SHA-256 fingerprints are recorded in `docs/addresses.md`.

## Build

The target is a 32-bit Windows DLL. GCC/MinGW-w64 is required because the
project uses GNU assembly and GCC calling-convention attributes; it does not
currently build with Visual Studio/MSVC.

### Fedora/Nobara

```sh
sudo dnf install mingw32-gcc-c++ mingw32-winpthreads-static
make
make test
```

### Windows (MSYS2)

Install the current 64-bit [MSYS2](https://www.msys2.org/) distribution, then
open its **MINGW32** shell. The shell itself runs on 64-bit Windows while its
compiler produces the 32-bit `i686` code required by Armada II.

Install Git, GNU Make, and the 32-bit GCC toolchain:

```sh
pacman -S --needed git make mingw-w64-i686-gcc
```

Clone and build the project from the same MINGW32 shell:

```sh
git clone https://github.com/TamsynnImogen/A2FOExtensions.git
cd A2FOExtensions
g++ -dumpmachine
make CXX_MINGW=g++ CXX_HOST=g++
make test CXX_HOST=g++
```

`g++ -dumpmachine` must report `i686-w64-mingw32`. Do not build from an
UCRT64, CLANG64, or other 64-bit shell: a 64-bit DLL cannot be loaded by the
32-bit game. The `make smoke` target is intended for Linux/Wine and is not
needed on Windows.

The DLL is written to `build/A2FOExtensions.dll`.

## Source guide

- `src/dllmain.cpp` contains version validation, runtime state, recursive ODF
  registration, and the evolver cocoon hooks. Start here for feature behavior.
- `src/delphi_bridge.S` translates between normal C++ calls, Fleet Ops'
  Delphi register convention, and Armada's 32-bit MSVC `thiscall` convention.
- `src/hook.cpp` provides the checked relative-call, relative-jump, and inline
  gateway patching primitives.
- `src/fpq_paths.cpp` parses only the metadata needed to discover ODF folders
  in an `odf.fpq`; it does not decompress archive payloads.
- `src/odf_paths.cpp` contains platform-independent basename normalization and
  fallback behavior covered by host tests.
- `src/startup_proxy.cpp` and `src/startup_proxy.def` load the companion early
  while preserving Armada's original startup-DLL exports.
- `docs/addresses.md` records supported hashes and all reverse-engineered RVAs.
- `tests/` contains the platform-independent parsers/path tests and the optional
  Windows DLL loading smoke-test program.

## Install

The cocoon hook must be installed before Armada builds its ODF class database.
For that reason the extension is loaded by a small proxy for Armada's existing
startup DLL, rather than through dxwrapper's later custom-DLL loader.

1. Copy `A2FOExtensions.dll` beside `ArmadaL.exe` in the game's `Data` folder.
2. In that folder, rename the shipped `Win2kDisableTaskSwitch.dll` to
   `Win2kDisableTaskSwitch.original.dll`. Keep this file: the proxy forwards
   Armada's two imports to it, and it still performs Fleet Operations' normal
   startup work.
3. Copy `Win2kDisableTaskSwitch.proxy.dll` into the folder and rename the copy
   to `Win2kDisableTaskSwitch.dll`.
4. Do not also load `A2FOExtensions.dll` through dxwrapper. If an earlier test
   added this line to `Data/dxwrapper.ini`, clear it:

   ```ini
   LoadCustomDllPath =
   ```

5. Start the game and inspect `Data/A2FOExtensions.log` if initialization fails.

To uninstall, delete the proxy `Win2kDisableTaskSwitch.dll`, rename
`Win2kDisableTaskSwitch.original.dll` back to `Win2kDisableTaskSwitch.dll`, and
remove `A2FOExtensions.dll`.

The project only changes process memory after checking the executable and hook
bytes. It never writes to `ArmadaL.exe` or `FleetOpsHook.dll` on disk.

## Resolution rules

- Only directories containing `.odf` files are added.
- New directories are ordered case-insensitively by their relative path.
- Duplicate basenames across different directories follow Fleet Operations'
  mod-level, primary-root, and loose-file-over-archive precedence. Directory
  order is only used to break an otherwise exact tie.
- Recursive selection applies only to bare `.odf` names and `.odf` requests
  within the virtual `odf` tree. A request without a recursive winner falls
  back to Fleet Operations' original hash result.
- A maximum of 227 new directories is accepted because Fleet Operations stores
  a virtual-directory order in one byte after its 28 built-in entries.
