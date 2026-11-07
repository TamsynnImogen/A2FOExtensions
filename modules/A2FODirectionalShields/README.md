# Directional shields

`A2FODirectionalShields.dll` optionally divides a Craft's native shield total
into multiple directional layouts. Existing Craft ODFs remain fully native
unless they explicitly enable the feature. Mode `1` is the original quad
forward/aft/port/starboard layout; mode `2` is tri-forward with equal 120-degree
forward/port/starboard sectors; mode `3` is tri-rear with equal 120-degree
aft/port/starboard sectors.

```cpp
maxShields = 650
directionalShields = 1
forwardShieldStrength = 200
aftShieldStrength = 150
portShieldStrength = 150
starboardShieldStrength = 150
```

`directionalShields = 1` selects quad shields, `directionalShields = 2`
selects tri-forward shields, `directionalShields = 3` selects tri-rear
shields, `directionalShields = 4` selects forward/rear hemispheres, and
`directionalShields = 5` selects port/starboard hemispheres. `0` or an
omitted command retains native shields.
For every active facing, a positive strength is explicit; an omitted strength
or `0` is automatic and receives an equal share of the capacity remaining in
`maxShields`. If any active facing is automatic, `maxShields` is required. If
all active strengths are explicit, an omitted `maxShields` is still derived
from their sum for A1-style content.

Example tri-forward policy:

```cpp
maxShields = 900
directionalShields = 2
forwardShieldStrength = 400
// port/starboard are omitted -> 250 each
```

The aft store is inactive (`0/0`) in mode 2. The forward store is inactive
(`0/0`) in mode 3.

Example tri-rear policy:

```cpp
maxShields = 900
directionalShields = 3
aftShieldStrength = 400
// port/starboard are omitted -> 250 each
```

`maxHealth` and `healthRate` remain Armada's hull pool and hull-repair rate;
the directional module never changes them. On an A2 ODF which explicitly
declares `maxShields`, the four facing strengths must add up to that value. A
mismatch rejects the directional policy rather than silently changing the
native A2 shield pool.

Armada 1 ODFs commonly omit `maxShields`. In that case the module installs the
four-facing total as the native shield maximum, preserving A1-style content
without requiring a conversion-only command. The total may be greater or
less than `maxHealth` because hull and shields are independent pools in A2.
The stock aggregate shield display, shield generator state, and `shieldRate`
continue to work. Native recharge is shared evenly between depleted facings.

Weapon hits use the attacker's position in the target Craft's local axes.
Forward/aft own exact diagonal boundaries; otherwise the dominant horizontal
direction selects port or starboard. Once a facing is depleted, subsequent
hits from that direction spill through to hull using Armada's normal damage
path even when another facing still has strength. Armada's native shield-
collapse effect is removed in the same damage transaction while the struck
facing is empty. The ordinary type-0 `xshldx01` weapon-impact flare is tracked
at `ShieldEffect::CreateShieldHit` using the facing retained by the immediately
completed damage call. A target-local hit matrix remains a fallback for callers
that provide a position; Armada's ordinary weapon path supplies an identity
matrix and creates its type-0 effect after `Craft::Damage` has returned.
Tracked effects on the depleted facing are stopped regardless of which native
or Fleet Operations ordnance subclass owns them, so later beam ticks cannot
keep refreshing a bubble over exposed hull. During effect creation, the module
briefly presents the impacted facing's percentage through the native aggregate
field and then writes the same percentage to the created effect's native
strength field. Tracked updates refresh that field directly, so Armada's stock
red-to-green effect gradient follows the arc rather than the sum of all arcs.
The aggregate is restored before returning and sidecar gameplay state is never
reconciled from this visual-only path. Type-1 collapse cleanup and a separately
enabled type-7 `alwaysShowShields` outline remain unaffected.

Damage without a resolvable weapon owner retains native aggregate-shield
behaviour. Its aggregate change is reconciled proportionally across the four
facings on the next Craft simulation boundary.

The module uses `A2FOWeaponDamageControls.dll` as the one checked owner of
`Craft::Damage`; select both modules in `info.ini`. Without that shared bridge,
the directional module retains no Craft sidecar state and changes no gameplay.
The two modules perform a late handshake in either load order, so their
`activeX` ordering does not change whether directional shields activate.

With `A2FOCraftIdentity.dll` selected, the single-selected-Craft panel shows
two diagnostic rows containing current/maximum values for F, A, P, and S.
Their optional GUI definitions are:

```text
infoSingleDirectionalShieldsForwardAftTextArea = 386 238 340 18
infoSingleDirectionalShieldsPortStarboardTextArea = 386 258 340 18
infoSingleDirectionalShieldsGraphicArea = 26 56 128 128
infoSingleDirectionalShieldsForwardValueTextArea = 58 78 64 18
infoSingleDirectionalShieldsAftValueTextArea = 58 144 64 18
infoSingleDirectionalShieldsPortValueTextArea = 46 111 44 18
infoSingleDirectionalShieldsStarboardValueTextArea = 90 111 44 18
directionalShieldColor = 0.1 1.0 0.1
directionalShieldLowColor = 1.0 0.5 0.0
directionalShieldCriticalColor = 1.0 0.05 0.02
directionalShieldValueColor = 0.8 1.0 0.8
directionalShieldValueLowColor = 1.0 0.7 0.1
directionalShieldValueCriticalColor = 1.0 0.15 0.05
```

Missing rectangles use positions relative to the live captain-name component.
Each Craft ODF may specify the four visible arc rectangles relative to the
directional graphic origin, in native UI units:

```cpp
forwardShieldPos = 26 0 76 20
aftShieldPos = 26 108 76 20
portShieldPos = 0 26 20 76
starboardShieldPos = 108 26 20 76
directionalShieldValueDisplayMode = 1
```

The format is `x y width height`; the values shown are defaults and form a
valid layout at 128-by-128. Explicit rectangles are used as supplied; they are
not normalized or scaled with the graphic dimensions and are not constrained
to `128`. Each command is optional and inherited independently. An omitted
segment uses its default 128-unit logical layout scaled to the configured
graphic area. A GUI per-facing `Area` override takes precedence over the ODF
rectangle.

`directionalShieldValueDisplayMode` controls the four independently placed
value labels: `0` hides them, `1` shows a rounded percentage number without a
`%` sign, and `2` shows `current/maximum`. Modes 1 and 2 keep the labels visible
alongside a successfully drawn ring. If the command is omitted, the previous
behaviour remains: a working ring replaces the two diagnostic rows, while a
missing ring falls back to those rows. Missing value rectangles use the
positions shown above relative to the graphic area. The `directionalShieldValue*Color`
commands colour the labels without changing the ring colours; omitted low or
critical colours fall back to the normal value colour and then the panel's
shared text colour.

Use `ART_CFG.h` to choose how those rectangles are presented globally:

```cpp
int directionalShieldDisplayMode = 1;
int directionalShieldForwardPosition = 0;
int directionalShieldAftPosition = 2;
int directionalShieldPortPosition = 3;
int directionalShieldStarboardPosition = 1;
```

Mode `1` uses the normal proportional drain; mode `2` keeps each segment full
and changes only its status colour, using black at exactly zero strength.
Position codes are `0 = north`,
`1 = east`, `2 = south`, and `3 = west`. The four positions must be unique.
Invalid or duplicate mappings safely retain the Craft ODF layout. Values are
read once from the extension-root chain when `A2FOCraftIdentity` loads, so a
child mod can override individual parent settings without a per-frame parser.

Each arc is green above 50%, orange from 25% through 50%, and red at or below
25%; the three optional colour commands override those defaults independently.
The legacy numeric fallback uses `directionalShieldColor`, then the selected
panel's text colour. The stock shield bar remains the aggregate of all four facings.
The graphic area is optional and supplies the origin and configured size for
the ring. Default segment placement uses a 128-by-128 logical layout scaled to
that area. Quad mode prefers a reusable horizontal/side pair:

```text
dsq  shield_quad      0 0 128 128
dsqs shield_quad_side 0 0 128 128
```

`dsq` is the forward/top segment and is vertically mirrored for aft. `dsqs` is
the port/left segment and is horizontally mirrored for starboard. The normal
facing placement/remapping still decides which physical edge each logical
facing occupies, so rotated quad layouts continue to use the corresponding
horizontal or vertical base sprite.

For compatibility, if either `dsq` or `dsqs` is unavailable, quad mode falls
back to the original four sprite definitions `dsf`, `dsb`, `dsl`, and `dsr`,
backed by `shield_forward.tga`, `shield_back.tga`, `shield_left.tga`, and
`shield_right.tga` respectively. Forward/back deplete horizontally from both
ends, while left/right deplete vertically from both ends. A dim full arc shows
the empty track and the bright portion eases to the current gameplay value over
about 450 ms. Missing both the compact pair and a complete legacy set leaves
the numeric diagnostics active as a fallback; a successfully drawn ring
replaces those text rows.

Tri modes additionally resolve these reusable sprites:

```text
dstc shield_tri_center 0 0 128 128
dsts shield_tri_side   0 0 128 128
```

`dstc` is the centre 120-degree segment and `dsts` is the port-side
120-degree segment. Mode `2` uses them directly for forward/port and mirrors
`dsts` horizontally for starboard. Mode `3` vertically mirrors the same
textures for aft/port and mirrors both axes for starboard. The existing
per-facing GUI/ODF rectangles still control each segment independently.

Define the entries directly in the loaded `gui_global.spr`, or in
a GUI-only table referenced from its initial include section, without
`@sprite_node` declarations. They are UI database sprites rather than world
scene nodes; a late include after sprite declarations may be ignored.
Preserve the sprite table's CRLF line endings: the legacy parser can merge
LF-only additions with adjacent records and corrupt their reference state.

Facing distribution is not yet appended to save data: after loading, the
saved aggregate shield percentage is distributed proportionally across all
four facings.


## Checkpoint — 2026-10-28: six render/UI families complete behind native adapters

This batch completes the remaining portable controllers and source/headless
validation for A2FOCraftIdentity (module and object editor),
A2FODirectionalShields, A2FOFeaturePack (recursive ODF, buildyard pseudo-technology,
queue enhancements and upgrade pods), A2FOShellDisplay (Bink and canvas),
A2FORGBTextures and A2FOTextureVariants: eleven inventory units in total.

Rust now controls CraftIdentity first-match fields/atlas lookup, full-byte string
trim, display-mode suffix admission, drawn predicates and visible hover setup;
RGB DDS/TGA publication traversal and precedence; FeaturePack ODF hash-chain
membership; directional effect-tree traversal; canvas ancestry, point-map pass
order and reverse lease selection; and texture scorch admission, publication
comparison and hardpoint selection. Native failure/reset paths and lazy reads
are preserved. Canvas ancestry releases each borrowed record before the next
lookup. Full u32 scorch bands and raw effect pointers remain data, separate
from callback error metadata. Display-mode suffix whitespace is classified
before the parsed-range decision, as in the original CRT path.

This is portable-controller completion, not a pure-Rust binary. Native code
retains Win32/COM/GDI/CRT and engine calls, hooks/signatures/addresses, checked
fields, native strings/maps/iterators/locks, exception scopes, exact GNU x87
projection/geometry/rounding primitives, and FeaturePack's existing STL sort.
The sort retains casefold-equivalent filename ordering and comparator/allocation
exception behavior. Object-editor and Bink controllers were already in Rust;
this batch supplies their current build and headless validation. Existing native
kernel files and ABI boundaries are pinned rather than rewritten speculatively.

Validation passed: 249 Rust tests; formatting, host/i686 Windows Clippy, both
release archives, integrated DLL and all six standalone DLLs. Twenty native
suites produced 267 matching before/current trace pairs (534 executions,
6,832 stdout lines), with inherited mutation and allocation-failure cases.
A direct production Bink fixture compares 276 numeric rows across 12 x87 modes;
canvas fixtures cover ancestry depth bounds, reverse/scoped leases and point
mapping. Seven additional directional tree cases cover signed branches, missing
and unreadable nodes, the head sentinel and cycle bound in both module forms.
A separate production scorch comparison passes 228 rows across all 12 x87
precision/rounding modes, including guards, boundary bands, modulo hardpoint
selection, publication state, raw sticky/C1 status, CW and stack balance.
Built-in batch9 admission also passes. Runtime fixture assets are isolated per
execution and retained; both sides see the same logical working-directory path.

The source boundary check preserves 56 complete native files and all text
outside 16 approved controller bodies plus new adapter helpers in six files.
All 417 source-referenced Rust implementations link (the verifier checks 420
including three defaults). Core 197 exports/ordinals and each standalone export
set remain identical, with no new system imports. The build pins 791 product
inputs including owned .inl files. Six objects depending on changed sources
were rebuilt; remaining cache entries and 7,851 third-party source pins were
verified against the passed Squadrons predecessor. Original/current fixtures
use independently pinned pre-batch and candidate Rust archives. Established
fixture-only GNU ABI/custom allocator diagnostic flags remain recorded in the
compile commands. Failed link/probe attempts are preserved.

Evidence: build/rust-migration-stage10/render-ui-batch/checkpoint/validation.json.
The eleven units are completed_pending_census. Official totals remain 116/151
with 35 uncredited units: seventeen completed units await the shared strict
census refresh, and eighteen units remain unfinished. Frozen membership,
baseline provenance and strict archive/common-DLL bindings are unchanged.
Canonical artifacts and earlier checkpoints are preserved. No deployment or
real game launch occurred; GPU and in-game acceptance remain pending.
