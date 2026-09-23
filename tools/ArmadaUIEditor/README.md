# Armada UI Editor

Armada UI Editor is a native visual editor for the editable interface layout
files used by **Star Trek: Armada** and **Star Trek: Armada II**. It reads the
games' `misc/gui_*.cfg` layouts together with their `Sprites/gui_*.spr` tables
and texture atlases.

## Features

- Dedicated Armada I and Armada II modes.
- Stock defaults for the Heroic installations at:
  - `/home/tamsynn/Games/Heroic/Star Trek Armada`
  - `/home/tamsynn/Games/Heroic/Star Trek Armada II`
- A1's 640x480 coordinate system and A2's declared `screenWidth` /
  `screenHeight` are handled independently.
- Recursive `#include` loading with effective-value precedence matching the
  game CFGs.
- Real panel previews assembled from SPR entries and TGA/DDS/PNG textures.
- Full-HUD, all-elements, and focused component views.
- Drag to move; drag the lower-right handle to resize; arrow keys nudge.
- Numeric rectangle inspector, search, grid/snap controls, undo/redo and reset.
- An **A2FO / FO** catalogue for additional resources, captain/registry,
  Photon and Quantum Torpedoes, Shuttle Craft, directional shields, shield
  hover, experience, and Fleet Operations system-display rectangles.
- RGB colour pickers for A2FO state colours and every three-channel
  `*Color`/`*Colour` value found in the loaded CFG chain.
- Fleet Operations `systembackgrounds.spr` previews inside
  `infoSingleSystemsDisplay`, with separate normal (16-slot) and compact
  (30-slot `Sm`) multi-selection canvas views.
- Saves every changed source CFG, including included CFGs when their effective
  entries were edited.
- Creates a sibling `.armada-ui-editor.bak` before first modifying each file.
- Preserves comments and unrelated values while normalising saved CFG files to
  CRLF line endings for legacy Armada and Fleet Operations compatibility.

The editor intentionally targets CFG layout rectangles. SPR atlas entries and
binary LDL databases are previewed/read as appropriate but are not rewritten
by this first version.

Open **A2FO / FO** in the left browser to add any missing extension rectangle
as an unsaved override in the active GUI CFG. Open **Colours** to add or edit
extension colours. Nothing is written until **Save All**; **Reset All** removes
new unsaved overrides as well as reverting edits. Mission-selector controls are
intentionally outside this HUD editor.

## Run

```bash
cargo run --release -- --a1
cargo run --release -- --a2
cargo run --release -- --a2 /path/to/misc/gui_fed.cfg
```

The application loads the selected mode's stock Federation HUD when no CFG is
given. Use **Choose Game Root** for another installation or an extracted mod.

## Headless validation

```bash
cargo run -- --inspect --a1
cargo run -- --inspect --a2
```

Inspection reports the resolved canvas, include/source count, rectangle count,
component groups, sprite count and resolved texture count without opening a
window.

## Build

```bash
cargo test --locked
cargo build --release --locked
```

## Medium and tall extension parts

The A2FO catalogue includes independent `infoSingle…` and `infoBuild…`
rectangles and colours for identities, ammunition labels/values/icons/bars,
shield segments/value labels, and XP. Missing tall fields inherit their medium
counterparts at runtime. Sprite names are edited in the CFG directly; the
editor previews bar geometry and colour, not custom bar textures. See
[the runtime key reference](../../docs/selected-panel-customisation.md).
