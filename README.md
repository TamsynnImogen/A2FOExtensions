# A2FOExtensions modular runtime

This package preserves the proven startup chain while separating checked engine
hooks, reusable dispatch, optional native features, and mod-authored Lua logic.

## Implemented

- `A2FOExtensions.dll` gains a versioned native-module ABI.
- Native modules are discovered from `modules\\*.dll` in deterministic filename order.
- The startup proxy attaches the renamed shipped startup DLL and our core
  immediately, preserving the proven timing of the recursive ODF and cocoon
  hooks. Native module discovery runs later on the core's post-attach worker,
  outside the Windows loader lock.
- Modules receive logging, Armada/Fleet Ops module handles, root-directory access,
  and checked inline/CALL/JMP patch helpers.
- API v2 provides a core-owned FOFS lookup dispatcher so modules do not compete
  to patch the same Fleet Operations instruction.
- `A2FOFeaturePack.dll` owns recursive loose-folder/FPQ discovery, the
  `wingman -> craft` compatibility alias, and the Evolver `cocoon` command.
- API v3 discovers the shared Data root plus the selected mod's `ParentMod`
  chain. Native DLLs and Lua scripts use deterministic basename overlay from
  Data through parents to the active mod.
- Lua 5.4.8 is embedded in the core, with memory/instruction/file-size limits
  and a deliberately restricted standard library.
- API v4 revision 1 adds capability discovery and a core-owned native
  destroyed-object dispatcher without breaking original v4 modules.
- Native and Lua startup registrations are transactional: a failed initializer
  leaves no callbacks or ownership records pointing into rejected code.
- The Lua API exposes real class-loading callbacks and a bounded temporary ODF
  view for mod-specific conditional logic. Built-in native features do not use
  token Lua registration scripts.
- Destroyed-object handlers declare the ODF fields they need. The core snapshots
  only the union of those fields, invokes native handlers before Lua, and accepts
  the first valid replacement.
- `A2FOFeaturePack.dll` adds ten-slot Ctrl-fill and experimental synchronized,
  save-persistent Ctrl+Alt continuous production.
- Fleet Ops mod `info.ini` files can set a first-run `DefaultGameSpeed` and
  redirect the per-mod `SettingsDirectory` without hard-coded mod paths.
- An SDK header and minimal example module are included.

## Core/module/script boundary

The core owns shared call sites, dispatch ordering, engine-object lifetimes, and
registration rollback. Native modules handle features that need deeper
engine/filesystem access, such as recursive ODF discovery, cocoon SOD selection,
and Producer integration. Lua scripts supply optional logic through narrow
semantic APIs when conditions and composition make scripting worthwhile. See
[`docs/architecture.md`](docs/architecture.md).

Queue controls and their current validation status are documented in
[`docs/queue-enhancements.md`](docs/queue-enhancements.md). Supported binary
identities and checked addresses are recorded in
[`docs/addresses.md`](docs/addresses.md).
The two optional Fleet Ops mod-information fields are documented in
[`docs/fleetops-info-defaults.md`](docs/fleetops-info-defaults.md).

## Expected runtime layout

```text
Armada II/
├── A2FOExtensions.dll
├── Win2kDisableTaskSwitch.dll
├── Win2kDisableTaskSwitch.original.dll
├── modules/
│   └── A2FOFeaturePack.dll
├── scripts/                 (optional modder scripts)
└── A2FOExtensions.log
```

The core retains its original log filename. Module and script messages are
prefixed in the shared log so startup order and overlay selection remain easy
to diagnose.

## Building on Nobara/Fedora

This project must be compiled as **32-bit Windows x86**, because Armada II and
Fleet Operations are 32-bit processes. Do not use the 64-bit MinGW compiler.

Install the toolchain:

```bash
sudo dnf install mingw32-gcc-c++ mingw32-binutils make
```

Build the release artifacts:

```bash
chmod +x build.sh
./build.sh
```

Or use Make directly:

```bash
make -j"$(nproc)"
make verify
make smoke
```

Outputs:

```text
build/A2FOExtensions.dll
build/Win2kDisableTaskSwitch.dll
build/modules/A2FOFeaturePack.dll
```

The SDK example is deliberately excluded from releases. Build and inspect it
separately with `make sdk-examples verify-sdk`.

The build links the MinGW runtime statically. `make verify` rejects outputs
that depend on deploy-time MinGW DLLs such as `libwinpthread-1.dll`; Armada's
`Data` directory should not need compiler runtime files added to it.
On Linux, `make smoke` also verifies under Wine that the core DLL loads and
exports `A2FO_Initialize`.

Before installing the proxy, rename the original shipped startup DLL:

```text
Win2kDisableTaskSwitch.dll
    -> Win2kDisableTaskSwitch.original.dll
```

Then copy the newly built `Win2kDisableTaskSwitch.dll`,
`A2FOExtensions.dll`, and the `modules` directory into the Fleet Operations
root directory.
