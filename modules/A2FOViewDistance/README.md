# A2FOViewDistance

Optional camera far-plane and object-culling distance adjustment for the
supported Armada II host. The release core now provides `A2FOViewDistance` inside
`A2FOExtensions.dll`; select it by its existing name in the `[modules]` list
in `Data/info.ini`, for example `activeN = "A2FOViewDistance"` at an unused row.
Stale standalone DLLs are ignored. The standalone build remains available for
SDK compatibility testing. See the [fifth built-in milestone](../../docs/builtin-modules-stage5.md).

The optional global API configuration is `Data/A2FOViewDistance.ini`:

```ini
[ViewDistance]
Enabled=0
MinimumDistance=10000
```

With no configuration, or with `Enabled=0`, the module is passive.
`MinimumDistance` must be finite and in the range 0 through 1,000,000. Zero is
a no-op. When enabled with a positive value, at the native configuration-parse
point the module raises the native finite, nonnegative far-plane and culling
limits to at least that value; it never lowers either limit. This happens before
the native restoration-baseline copy. Mission scripts can override culling
later. The INI is read once during preparation; changing it requires a
whole-process restart. The module menu gates adjustments at future native
parser visits. Disabling the module on shutdown prevents future adjustments; it cannot undo values already parsed
in the running process.

The module does not edit `RTS_CFG.h`, `ART_CFG.h`, or the executable on disk.
It uses the existing API root at runtime. Hook sites are admitted only for the
existing ArmadaL compatibility profile and the pinned GOG image. The PE's
preferred image base is `0x400000`, but its actual loaded `HMODULE` may differ.
The module validates the image and signatures by RVA. For each checked absolute
operand, it requires that operand's PE `HIGHLOW` relocation and adjusts only
that operand by the load delta; all other signature bytes must match exactly.
Bridge data pointers are derived from the actual image base plus their RVAs. A
foreign hook owner leaves the module passive.

The standalone diagnostic `A2FORefit_QueryStatus` export
(`A2FOBuiltinViewDistance_QueryStatus` in the core) distinguishes an installed hook
from an armed adjustment and reports `parser_visits` separately. Installation
does not prove the native configuration parser has visited the bridge;
`parser_visits` counts bridge entries and does not itself prove a visible
in-game result.

The implementation is an independent checked integration into the existing
core, adapting the view-distance idea in upstream `qol/qol.c`; it is not a
wholesale integration of upstream QOL. See the [upstream source and license
record](../../third_party/armada2-refit/SOURCE.md) and [license](../../third_party/armada2-refit/LICENSE.txt).

Build and headless checks:

```sh
make refit-modules
make refit-modules-test
make refit-modules-signatures
```

The native fixture uses the actual DLL bridges without executing an engine
entry point. Run it only with an existing disposable Wine prefix:

```sh
make refit-modules-native-test REFIT_WINEPREFIX=/absolute/path/to/disposable-prefix
```

The 18 October 2026 fixture run passed all 27 console processes across the
local ArmadaL, clean GOG and installed Roots inputs. See the
[verification record](../../docs/refit-camera-view-2026-10-18.md) for the
exact scope and evidence. It does not run game startup.

Native game acceptance remains pending. Manually check maximum camera zoom,
fog and hidden objects, then verify a mission script can reset culling distance.
Restart the whole process when changing settings.
