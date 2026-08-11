# A2FO Arc Lab

A2FO Arc Lab is a standalone visual authoring and diagnostic tool for
`A2FOFireArcs`. It loads a ship or station ODF, resolves its include and
`ParentMod` chains, displays the corresponding Armada SOD, discovers every
`weaponX`/`weaponHardpointsX` link, and draws the selected weapon's permission
volume at its linked hardpoints.

The angle test is not a second approximation of the extension. The Rust UI
calls the same `modules/A2FOFireArcs/fire_arc.cpp` implementation that is
compiled into `A2FOFireArcs.dll`.

## Using the tool

Open the application and choose **Open Ship ODF**, or pass a loose ship ODF on
the command line:

```text
A2FOArcLab.exe "C:\Games\Fleet Ops\Data\Mods\My Mod\odf\ships\myship.odf"
```

```bash
./A2FOArcLab "/games/Fleet Ops/Data/Mods/My Mod/odf/ships/myship.odf"
```

The left panel selects a discovered weapon slot. The right panel provides:

- all-hardpoints and single-hardpoint views;
- previous/next hardpoint cycling (`[` and `]` also work);
- box and cone modes with live angle editing;
- dorsal, ventral, forward, rear, port, and starboard presets;
- derived coverage and warnings for easy-to-miss hemisphere overlap;
- a movable target probe whose ALLOWED/BLOCKED result comes from the DLL
  geometry;
- copy and save actions for the generated weapon-ODF block.

Right-drag orbits the camera, middle-drag pans, and the mouse wheel zooms.
The coloured axes and orientation legend use the extension's convention:

```text
+X = starboard/right   +Y = dorsal/up   +Z = forward
```

Fire-arc orientation is owner/ship-local, exactly as it is at runtime. Each
linked hardpoint supplies the visual origin, but rotating a hardpoint in the
SOD does not rotate an ordinary weapon's permission volume.

## Diagnostic inspection

The headless inspection mode verifies resolution and prints the detected model,
weapon ODFs, hardpoints, and normalized custom arcs without opening a window:

```bash
./A2FOArcLab --inspect "/path/to/odf/ships/myship.odf"
```

It also parses the resolved SOD and warns when a configured weapon hardpoint is
absent from the model.

## Resolution rules

The selected loose ODF establishes the active asset root. Arc Lab then follows
`ParentMod` declarations from `info.ini` and finally searches the shared Data
root, preserving active-mod-first precedence. File lookup is case-insensitive
on both Windows and Linux and searches nested ODF/SOD directories. Textures are
resolved from `Textures/RGB`, `Textures/Index8`, `Textures/Compressed`, the
general `Textures` directory, and SOD directories in the same root order.

ODF includes and multi-line list values are supported. SOD versions 1.4 through
1.93 are accepted. If a model cannot be resolved from `baseName` or the ODF
filename, use **Choose SOD**.

Arc Lab currently reads loose files, not content stored only inside FPQ
archives. Extract the relevant ODF/SOD/texture files or place loose overrides
in the mod while authoring. It intentionally does not rewrite inherited weapon
ODFs; copy or save the generated block and place it in the file you control.

## Building

Rust stable and a C++ compiler are required because the exact portable
`fire_arc.cpp` implementation is compiled into the application.

Linux requires `pkg-config`, `g++`, and the usual X11 runtime libraries. Audio
and gamepad support are deliberately excluded, so Arc Lab does not depend on
ALSA or libudev. From this directory:

```bash
cargo test --locked
cargo build --release --locked
./scripts/package-linux.sh
```

On 64-bit Windows, install Rust using the MSVC toolchain and Visual Studio Build
Tools with C++ support, then run in PowerShell:

```powershell
cargo test --locked
cargo build --release --locked
.\scripts\package-windows.ps1
```

Packages are written to the repository-level `dist` directory. The repository
workflow `.github/workflows/arclab.yml` performs the same release build on
Ubuntu and Windows and uploads both archives as artifacts.

## Scope

Arc Lab visualizes permission geometry; the game still applies weapon range,
target validity, obstruction, technology, and other firing checks. SOD
animations are deliberately not played, and the model renderer is intended for
diagnostics rather than exact in-game lighting reproduction.
