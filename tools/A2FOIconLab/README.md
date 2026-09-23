# A2FO Icon Lab

A2FO Icon Lab is a standalone visual editor for Fleet Operations
`weaponXiconpos` commands. It resolves a loose ship or station ODF through its
include and `ParentMod` chains, finds the matching `<model>_si` entry in the
active `systembackgrounds.spr`, maps the ship's faction to its interface CFG
and SPR, and previews the result in that faction's information panel.

Each discovered `weapon1` through `weapon128` uses its real `i_<weapon>` sprite
from `systemimages.spr`. Slots 33–128 match A2FOCraftIdentity's extended
selected-panel controls; unmodified Fleet Operations displays slots 1–32. If
no explicit icon exists, the tool uses `systemicon_default`, matching Fleet
Operations. Weapon icons are drawn at the faction CFG's
`infoSingleSystemsIcon` width and height, matching the in-game selected panel;
the SPR crop size is used only when that CFG rectangle is absent.

## Using the tool

Open an ODF from the application or pass it on the command line:

```text
A2FOIconLab.exe "C:\Games\Fleet Ops\Data\Mods\My Mod\odf\ships\myship.odf"
```

```bash
./A2FOIconLab "/games/Fleet Ops/Data/Mods/My Mod/odf/ships/myship.odf"
```

Choose a weapon slot, or click its icon in the preview, then click on the ship's
system background to place it. You can also drag an existing icon directly.
The editor uses whole-percent coordinates with `0 0` at the top-left and
`100 100` at the bottom-right. The icon is centred on that point.

**Save All** or `Ctrl+S` updates existing commands in the opened ship ODF. If a
position came from an included ODF, the editor appends a local override instead
of modifying the inherited file. Before the first change it creates
`<ship>.odf.a2fo-iconlab.bak`; later saves retain that original backup.
Every ODF written by the editor is normalised to CRLF line endings for legacy
Armada and Fleet Operations parser compatibility.

The editor reads loose assets. If the ODF is outside its mod directory, use
**Choose Asset Root** and select the mod folder or shared `Data` folder. Assets
that exist only inside an FPQ must be extracted or supplied as loose overrides
while authoring.

## Building

From this directory:

```bash
cargo test --locked
cargo build --release --locked
```

The application intentionally excludes audio and gamepad support. Linux builds
use X11. A 64-bit Windows build can be produced from Linux when the GNU target
and MinGW compiler are installed:

```bash
rustup target add x86_64-pc-windows-gnu
cargo build --release --locked --target x86_64-pc-windows-gnu
```

The Windows executable is written to
`target/x86_64-pc-windows-gnu/release/a2fo_iconlab.exe` and uses the Windows GUI
subsystem, so launching it does not open a separate console window. The source
also builds with the normal Rust MSVC toolchain on Windows.

Asset resolution can also be checked without opening a window:

```bash
./A2FOIconLab --inspect "/path/to/odf/ships/myship.odf"
```
