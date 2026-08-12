# Three-dimensional weapon fire arcs

`A2FOFireArcs.dll` gives an ordinary weapon an optional firing volume which is
fixed to its owner. It answers one question: **is the target direction inside
the volume from which this weapon is allowed to fire?**

The arc does not command the ship to move, tilt, or rotate. If the target is
already inside the volume, the weapon may fire while its owner is stationary
or has disabled engines. Native target-validity, range, and obstruction checks
still apply.

Put these commands in the **weapon ODF**, such as `fbphas.odf`. Do not put them
in its ordnance ODF, such as `fbphaso.odf`.

## The short version

- Use `box` for most phaser arrays, broadsides, and hemispherical coverage.
- Use `cone` for a circular fixed-cannon, torpedo, or barrel-like field.
- `fireArcYaw` selects the horizontal centre direction.
- `fireArcPitch` selects the vertical centre direction.
- Every angle/width is measured in degrees.
- `fireArcYawAngle`, `fireArcPitchAngle`, and `fireArcAngle` are total widths.
  A value of `90` reaches 45 degrees to either side of its centre.
- Yaw wraps around the ship; pitch is limited to `-90..+90`.
- Weapons containing none of these commands retain native Fleet Operations
  `restrictFireArc`/`fireArc` behaviour.

For a broad upper firing volume:

```cpp
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = 90
fireArcYawAngle = 270
fireArcPitchAngle = 180
```

For the matching lower volume, change only `fireArcPitch` to `-90`.

## Orientation

Yaw is a compass around the owner's horizontal plane:

```text
                         Forward
                         yaw 0
                           ^
                           |
       Port / left <--- [ owner ] ---> Starboard / right
       yaw 270 or -90                  yaw 90
                           |
                           v
                          Rear
                         yaw 180
```

Yaw wraps, so equivalent values include:

| Yaw | Direction | Equivalent value |
| ---: | --- | ---: |
| `0` | forward | `360` |
| `90` | starboard/right | `-270` |
| `180` | rear | `-180` |
| `270` | port/left | `-90` |

Pitch is an elevator above or below that horizontal direction:

```text
 +90 = directly above
 +45 = upward diagonal
   0 = level
 -45 = downward diagonal
 -90 = directly below
```

Pitch does not wrap. Values above `90` clamp to `90`; values below `-90`
clamp to `-90`. Consequently, `fireArcPitch = 270` means up, not down. To
describe a direction which has passed over the top toward the rear, use a rear
yaw with a positive pitch—for example, yaw `180`, pitch `45`.

Yaw and pitch combine naturally:

| Centre direction | Yaw | Pitch |
| --- | ---: | ---: |
| forward and up | `0` | `45` |
| right and up | `90` | `45` |
| left and up | `270` | `45` |
| rear and down | `180` | `-45` |
| right and down | `90` | `-45` |

At exactly `+90` or `-90` pitch, yaw is geometrically irrelevant because the
direction is directly above or below.

## Centres and widths

The centre commands choose where the volume points. The angle commands choose
how much it covers around that centre.

For this box:

```cpp
fireArcYaw = 0
fireArcPitch = 0
fireArcYawAngle = 90
fireArcPitchAngle = 60
```

the permitted offsets are:

```text
yaw:   -45 through +45 degrees around forward
pitch: -30 through +30 degrees around level
```

Boundaries are inclusive. Widths are never interpreted as “90 degrees on each
side”; a width of `90` is 45 degrees on each side.

## Box mode

Box mode tests yaw and pitch independently:

```cpp
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = 0
fireArcYawAngle = 90
fireArcPitchAngle = 60
```

It forms a rectangular angular window. A target is allowed when both its yaw
and pitch fall inside their respective widths. This makes box mode useful when
horizontal and vertical coverage should differ.

Missing yaw coverage defaults to `360`; missing pitch coverage defaults to
`180`. You can therefore declare only the dimension which needs restricting.

### Upper and lower hemispheres

This covers level through directly above, while retaining a 270-degree yaw
window:

```cpp
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = 90
fireArcYawAngle = 270
fireArcPitchAngle = 180
```

This covers level through directly below:

```cpp
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = -90
fireArcYawAngle = 270
fireArcPitchAngle = 180
```

The level boundary is included in both examples. A 180-degree pitch width is
fully unrestricted only when centred at `0`; when centred at `+90` or `-90`,
it selects the upper or lower hemisphere respectively.

### Two-dimensional compatibility spelling

The shorter form creates a horizontal box with unrestricted pitch:

```cpp
fireArcCenter = 90
fireArcWidth = 60
```

This is centred on starboard and covers 30 degrees to either side. It is
equivalent to:

```cpp
fireArcMode = "box"
fireArcYaw = 90
fireArcYawAngle = 60
```

If both forms are present, `fireArcYaw` overrides `fireArcCenter` and
`fireArcYawAngle` overrides `fireArcWidth`.

## Cone mode

Cone mode measures the true three-dimensional angular distance from one centre
direction:

```cpp
fireArcMode = "cone"
fireArcYaw = 0
fireArcPitch = 0
fireArcAngle = 90
```

The `90` is the cone's total diameter, so the target may be at most 45 degrees
from its centre in any direction. The cross-section is circular rather than a
box with independent corners.

For example, a target at yaw `40`, pitch `40` is inside a 90-by-90 box because
both individual offsets are below 45 degrees. It is outside a 90-degree cone
because the combined three-dimensional offset exceeds 45 degrees.

Cone mode uses `fireArcAngle`; it does not use `fireArcYawAngle` or
`fireArcPitchAngle`. Supplying `fireArcAngle` without `fireArcMode` also selects
cone mode.

## Command reference

| Command | Mode | Meaning | Default/validation |
| --- | --- | --- | --- |
| `fireArcMode` | both | `"box"` or `"cone"` | box unless `fireArcAngle` selects cone |
| `fireArcYaw` | both | horizontal centre | `0`; wraps into `-180..180` |
| `fireArcPitch` | both | vertical centre | `0`; clamps to `-90..90` |
| `fireArcYawAngle` | box | total horizontal width | `360`; clamps to `0..360` |
| `fireArcPitchAngle` | box | total vertical width | `180`; clamps to `0..180` |
| `fireArcAngle` | cone | total circular diameter | required; clamps to `0..360` |
| `fireArcCenter` | box alias | legacy horizontal centre | overridden by `fireArcYaw` |
| `fireArcWidth` | box alias | legacy total horizontal width | overridden by `fireArcYawAngle` |

A box must declare at least one width command: `fireArcWidth`,
`fireArcYawAngle`, or `fireArcPitchAngle`. A centre by itself is incomplete.
A cone always requires `fireArcAngle`. Malformed or incomplete custom settings
are logged and leave that WeaponClass on its native arc path.

## Global enable switch

The module is enabled by default. Add this to `RTS_CFG.h` only when a mod needs
to override that default:

```cpp
int firearc = 1;
```

- `firearc = 1` enables custom box/cone firing enforcement and the weapon-icon
  hover preview.
- `firearc = 0` disables the complete module. Weapons then use only their
  native Fleet Operations `restrictFireArc`/`fireArc` behaviour and no A2FO
  hover wireframe is drawn.

The extension reads `RTS_CFG.h` from Data, each parent mod, and the active mod
in normal overlay order. A later valid `0` or `1` overrides an earlier value.
An absent setting inherits the previous value; an invalid value is ignored and
reported in `A2FOExtensions.log`.

## In-game weapon-icon preview

When a selected craft exposes a weapon through `weaponXiconpos`, move the mouse
over that existing system icon to preview the weapon's configured A2FO arc in
the tactical view. The preview disappears as soon as the pointer leaves the
icon; clicking is not required and the icon's normal tooltip/click behaviour is
unchanged.

The DLL reads the hovered icon's real zero-based weapon slot, then walks that
live Weapon instance's linked hardpoint list. It draws one wireframe volume from
every associated hardpoint:

- cyan lines show the boundary and its corner/edge rays by default;
- the centre direction is gold by default;
- the complete wireframe turns green by default while the Weapon's live target
  is inside the configured volume;
- an unrestricted volume is shown as a wireframe sphere;
- the drawing uses a bounded fraction of weapon range, so it communicates arc
  direction and width rather than claiming to be a range circle.

The three colours are optional UI configuration entries. Put them alongside
the other colour commands in the active interface `.cfg` file:

```ini
fireArcBoundaryColor    = 0.10 0.90 1.00
fireArcCenterColor      = 1.00 0.82 0.12
fireArcValidTargetColor = 0.15 1.00 0.20
```

Each channel is a floating-point value from `0` to `1`. Missing entries retain
the defaults shown above. The valid-target colour replaces both boundary and
centre colours while the current target is geometrically inside the volume.

Directions use the weapon owner's live right/up/forward axes—the same axes used
by firing authorization—while each wireframe begins at its real hardpoint world
position. A mounted turret therefore previews its rotated live direction when
its own weapon icon is available. If Armada supplies no usable hardpoint list,
the DLL falls back to the owner centre.

Only weapons containing a valid custom A2FO box/cone policy receive this
preview. Weapons using only native `restrictFireArc`/`fireArc` retain native
firing and do not show an extension wireframe, because those stock values do
not describe the complete three-dimensional volume.

The green state means the weapon's current live target is geometrically inside
the custom arc. It deliberately does not call combat authorization from the UI
renderer, so range, obstruction, technology, reload, and autonomy may still
prevent an immediate shot.

## Runtime behaviour

For a configured ordinary weapon, firing proceeds in this order:

```text
Fleet Operations target validity
  -> native range check
  -> native obstruction check
  -> custom complete yaw/pitch box or cone
  -> trigger-time repeat of the same custom volume
  -> normal-weapon technology-tree filter, if installed
  -> native shot
```

The custom volume replaces only Armada's stock two-dimensional
`restrictFireArc`/`fireArc` direction gate. The native value is ignored for a
WeaponClass with a valid custom policy and remains untouched for every other
weapon.

The complete volume is checked during target authorisation and again just
before `Weapon::Trigger`. The second check prevents a shot if the owner or
target moves across the boundary between those two engine stages.

`A2FOTurrets.dll` owns the shared trigger hook used by this last check. It must
be installed with `A2FOFireArcs.dll`. A linked hull turret is itself the weapon
owner, so its firing volume follows the turret's live yaw and pitch rather than
the parent hull.

If `A2FONormalWeaponTech.dll` is installed, its check runs after the fire-arc
filter. A log saying that the 3D arc allowed a trigger followed by a technology
entry blocking the project means the arc worked: satisfy or temporarily remove
that `.tt` prerequisite when testing geometry.

## Common mistakes

- Putting the commands in the ordnance ODF. They belong in the weapon ODF.
- Treating an angle as a per-side amount. Every width is the complete width.
- Using `fireArcYawAngle`/`fireArcPitchAngle` with cone mode instead of
  `fireArcAngle`.
- Using pitch values such as `180` or `270` to mean “past the top”. Pitch
  clamps at `+90`; combine a rear yaw with a smaller positive pitch instead.
- Assuming an arc turns or pitches the ship. It grants or rejects a shot; it
  does not issue movement orders.
- Forgetting a weapon's active technology-tree prerequisite during testing.
- Testing only one viewpoint. Because the volume is owner-local, turn the
  owner and confirm the volume turns with it.

## Multiplayer and fallback

Fire-arc decisions affect simulation. Every multiplayer participant must use
the same DLLs and weapon ODF values.

Weapons without custom commands retain Fleet Operations' native behaviour.
Invalid custom commands also fall back to the native path rather than leaving
the weapon partially configured.
