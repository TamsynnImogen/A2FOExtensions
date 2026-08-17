# Directional shields

`A2FODirectionalShields.dll` optionally divides a Craft's native shield total
into forward, aft, port, and starboard facings. Existing Craft ODFs remain
fully native unless they explicitly enable the feature.

```cpp
maxShields = 650
directionalShields = 1
forwardShieldStrength = 200
aftShieldStrength = 150
portShieldStrength = 150
starboardShieldStrength = 150
```

`directionalShields = 1` is the activation gate. Merely declaring one or more
strength fields does not enable the feature. All four strengths must be finite
and greater than zero; otherwise the class is rejected and retains native
shield behaviour.

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
directionalShieldColor = 0.1 1.0 0.1
directionalShieldLowColor = 1.0 0.5 0.0
directionalShieldCriticalColor = 1.0 0.05 0.02
```

Missing rectangles use positions relative to the live captain-name component.
Each Craft ODF may override the four visible arc rectangles inside the
128-by-128 graphic area:

```cpp
forwardShieldPos = 26 0 76 20
aftShieldPos = 26 108 76 20
portShieldPos = 0 26 20 76
starboardShieldPos = 108 26 20 76
```

The format is `x y width height`; the values shown are the defaults. Each
command is optional and inherited independently. Positive width and height
resize the corresponding segment as well as its depletion bounds.

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
The numeric fallback uses `directionalShieldColor`, then the selected panel's
text colour. The stock shield bar remains the aggregate of all four facings.
The graphic area is an
optional fixed-size 128-by-128 origin for four sprite definitions named
`dsf`, `dsb`, `dsl`, and `dsr`, backed by the corresponding
`shield_forward.tga`, `shield_back.tga`, `shield_left.tga`, and
`shield_right.tga` files. Forward/back deplete horizontally from
both ends, while left/right deplete vertically from both ends. A dim full arc
shows the empty track and the bright portion eases to the current gameplay
value over about 450 ms. Missing graphic sprites leave the numeric diagnostics
active as a fallback; a successfully drawn ring replaces those text rows.
Define the four entries directly in the loaded `gui_global.spr`, or in
a GUI-only table referenced from its initial include section, without
`@sprite_node` declarations. They are UI database sprites rather than world
scene nodes; a late include after sprite declarations may be ignored.
Preserve the sprite table's CRLF line endings: the legacy parser can merge
LF-only additions with adjacent records and corrupt their reference state.

Facing distribution is not yet appended to save data: after loading, the
saved aggregate shield percentage is distributed proportionally across all
four facings.
