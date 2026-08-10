# Three-dimensional weapon fire arcs

`A2FOFireArcs.dll` adds optional, owner-local box and cone firing volumes to
ordinary weapon ODFs. Weapons without any commands documented here retain
Fleet Operations' existing `restrictFireArc`/`fireArc` behavior.

All new angles are degrees. Yaw uses the weapon owner's local axes:

```text
0   = forward
90  = starboard
180 = rear
270 = port
```

Pitch is `0` level, `90` up, and `-90` down. The firing volume follows the
owner as it turns. A linked A2FOTurrets object is the weapon owner, so its arc
also follows the turret's live yaw and pitch.

## Box mode

```text
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = 0
fireArcYawAngle = 90
fireArcPitchAngle = 60
```

This accepts targets up to 45 degrees left/right and 30 degrees up/down from
the owner's forward axis. The boundary is inclusive. Missing yaw coverage is
360 degrees and missing pitch coverage is 180 degrees, allowing a horizontal-
only or vertical-only restriction.

The shorter two-dimensional spelling is an alias for a horizontal box:

```text
fireArcCenter = 90
fireArcWidth = 60
```

That creates a starboard-facing arc with 30 degrees of coverage to either
side and unrestricted pitch. If both spellings are present, `fireArcYaw`
overrides `fireArcCenter`, and `fireArcYawAngle` overrides `fireArcWidth`.

`fireArcMode` may be omitted for box mode. A box must declare at least one of
`fireArcWidth`, `fireArcYawAngle`, or `fireArcPitchAngle`; a centre alone is
treated as incomplete and leaves native behavior active.

## Cone mode

```text
fireArcMode = "cone"
fireArcYaw = 0
fireArcPitch = 0
fireArcAngle = 90
```

This is a true 90-degree cone (45 degrees from its centre in every direction),
which differs from the corner of a 90-by-90 box. Supplying `fireArcAngle`
without `fireArcMode` also selects cone mode. Cone mode always requires
`fireArcAngle`.

Centres are normalized/clamped to yaw `-180..180` and pitch `-90..90`.
Coverage is clamped to yaw/cone `0..360` and pitch `0..180`. Malformed or
incomplete custom settings are logged and leave that weapon on its native
firing-arc path.

When a valid custom arc is present, the module applies its horizontal envelope
after Armada's native range, obstruction, and stock-arc checks. This lets the
ordinary attack AI turn the craft into yaw coverage. The complete box or cone,
including pitch, is checked immediately before `Weapon::Trigger` through the
shared A2FOTurrets weapon hook. A target outside pitch coverage suppresses the
shot without asking Armada's two-dimensional movement AI to manoeuvre
vertically; the weapon waits until the target enters its 3D volume naturally.

`restrictFireArc` does not need to be set for the custom arc. If it is set
explicitly, both the stock arc and custom arc must accept the target. The
standard extension build includes `A2FOTurrets.dll`, which is required for the
late full-3D trigger filter. Without it, only the safe horizontal envelope can
be enforced. All multiplayer participants must use the same DLL and ODF
values.
