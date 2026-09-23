# A2FO Tooltip Lab

A2FO Tooltip Lab is a standalone editor for Fleet Operations
`Dynamic_Localized_Strings.h` files. It provides searchable entries, a
multiline tooltip editor, validation, and a live preview made from the same
sprite font and texture atlas that the opened mod uses.

Fleet Operations' red numbers, resource symbols, `(Avatar)`, `(passive)`, unit
class labels, and similar text are not markup or Unicode. They are custom
single-byte font glyphs. Their byte assignments can differ between mods. The
lab therefore resolves `Sprites/FontSmall.spr` and its texture through the
opened mod's `info.ini` `ParentMod` chain and creates the insert palette from
that actual mapping. Roots, Fleet Ops 4.0, and custom mods are not assumed to
share one global table.

## Using the tool

Open a file in the application or pass it on the command line:

```text
A2FOTooltipLab.exe "C:\Games\Fleet Ops\Data\Mods\My Mod\Dynamic_Localized_Strings.h"
```

```bash
./A2FOTooltipLab "/games/Fleet Ops/Data/Mods/My Mod/Dynamic_Localized_Strings.h"
```

Select an existing key or create a new one. The editor represents custom game
glyphs with readable tokens such as `[[81:Red 1]]`. For example, the verified
FO4 profile uses `[[B6:(Avatar)]]`, while the verified Roots profile uses
`[[B7:(Avatar)]]`.
Expand **Special characters from this mod** and click a rendered glyph to
insert its correct byte at the caret. Tokens are converted back to raw
single-byte Fleet Ops text on save.

**Import ODFs** loads one or more `.odf` files and discovers every
`*Tooltip` assignment, including `tooltip`, `verboseTooltip`, and mod-specific
variants such as `officerTooltip`. Duplicate references are grouped, keys
already present in the open DLS are identified, and missing key-like values
are preselected. Literal ODF tooltip text remains visible but is not
automatically mistaken for a localization key. Review and edit the initial
text, add the selected entries, then use **Save All** when ready.

**Glyph Builder** creates a new one-byte phrase glyph from characters in the
active mod's own font. Choose a byte from `0x7F` through `0xFF`, enter a
palette label and displayed phrase, review its source-font preview, and
explicitly confirm replacement of that byte. The builder finds unused
transparent atlas space, copies the exact existing glyph pixels, updates the
matching UV and width keyframes in `FontSmall.spr`, and refreshes the palette.
It supports TGA and PNG atlases; DDS remains preview-only. Before editing, it
creates one-time `.a2fo-tooltip-lab.bak` copies of both font assets. If a mod
inherits its font, local `Sprites` and `Textures` overrides are created in the
opened mod so the parent mod remains untouched. A manually selected Font / Mod
Root is treated as an intentional direct-edit target.

The preview uses the resolved font atlas, byte mapping, widths, and line
height. It also exposes a wrap-width control. If a standalone file cannot be
associated with its assets automatically, choose **Font / Mod Root**.

**Save All** or `Ctrl+S` changes only edited value spans and newly added
entries. Comments and unrelated entries remain untouched; saved legacy text is
normalised to CRLF line endings for Fleet Operations compatibility. Before the
first change, the tool creates
`Dynamic_Localized_Strings.h.a2fo-tooltip-lab.bak`; later saves retain that
original backup. Unsupported Unicode, malformed glyph tokens, duplicate new
keys, and `MAX_STRINGS` overflow are rejected before writing.

## Inspection and building

The mod/font resolution and available glyph mappings can be checked without a
window:

```bash
./A2FOTooltipLab --inspect "/path/to/Dynamic_Localized_Strings.h"
```

Build and test from this directory:

```bash
cargo test --locked
cargo clippy --locked --all-targets -- -D warnings
cargo build --release --locked
```

A 64-bit Windows build can be produced from Linux when the GNU target and
MinGW compiler are installed:

```bash
cargo build --release --locked --target x86_64-pc-windows-gnu
```
