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

On Fedora/Nobara:

```sh
sudo dnf install mingw32-gcc-c++ mingw32-winpthreads-static
make
make test
```

The DLL is written to `build/A2FOExtensions.dll`.

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
