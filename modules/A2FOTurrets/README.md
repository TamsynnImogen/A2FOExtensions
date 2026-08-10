# Indexed hull-mounted turrets

`A2FOTurrets.dll` adds linked, independently armed turret objects to ordinary
Craft-derived objects. Each turret is a native object with its own SOD,
weapons, hitpoints, target selection, and team, while the module keeps it on a
parent hardpoint and rotates it toward its weapon target.

This is an experimental first implementation and still requires manual
in-game validation.

## Parent ODF

Declare matching indexed pairs on the parent ship or station:

```text
turret0 = "bsg_dual_turret"
turretHardpoint0 = "hp_turret00"

turret3 = "bsg_dual_turret"
turretHardpoint3 = "hp_turret03"
```

Indices 0 through 64 are supported and may be sparse. `turret1` and `turret2`
do not need to exist for `turret3` to work. A pair with only one field is
logged and skipped. The turret value is an ODF basename; a trailing `.odf` is
accepted but not required.

The parent SOD hardpoint's local forward axis (`+Z`) defines zero yaw and zero
pitch. Orient each mount accordingly in the model.

## Turret ODF

The referenced object must use the semantic turret classlabel:

```text
#include "turret.odf"

classLabel = "turret"
unitName = "Dual turret"
tooltip = "Dual turret"
verboseTooltip = "Dual turret"
race = "federation"
physicsFile = "smoothphys.odf"

maxHealth = 300
shieldProtection = 0

turretYawMin = -180
turretYawMax = 180
turretPitchMin = -10
turretPitchMax = 85
turretYawRate = 90
turretPitchRate = 60
turretRestYaw = 0
turretRestPitch = 0
turretReturnToRest = 1

weapon1 = "bsg_turret_cannon"
weaponHardpoints1 = "hp01" "hp02"
```

Including a complete stock/faction turret ODF is strongly recommended. For
example, a Federation test object may include `fturret.odf` instead of the
generic `turret.odf`, then override `classLabel = "turret"` afterward. A file
containing only the rotation fields and classlabel lacks the ordinary native
station, race, health, and physics defaults needed by a useful child object.

Angle fields are degrees. Rate fields are degrees per second. The values shown
above are also the defaults when a field is absent. Yaw is limited to
`-180..180`, pitch to `-90..90`, and negative rates become zero. If a minimum
is greater than its maximum, the module swaps them.

`classLabel = "turret"` is mapped internally to Armada's native `sensor`
class. Missing-only defaults hide the linked object from the interface, set
`avoidMe = 0` so its parent and nearby craft do not steer around the object,
and disable its footprint/avoidance bookkeeping. Explicit values in the
turret ODF or its include chain remain authoritative.

The turret's weapon ODF can use Armada's existing `restrictFireArc` and
`fireArc` commands as an additional firing restriction. With
`A2FOFireArcs.dll`, it can instead use an owner-local box or cone which follows
the turret's live orientation. See
[`../A2FOFireArcs/README.md`](../A2FOFireArcs/README.md).

The turret module's existing `Weapon::Trigger(GameObject)` hook also hosts
A2FOFireArcs' final full-3D trigger filter and A2FONormalWeaponTech's ordinary
weapon technology filter. This keeps vertical rejection out of Armada's
two-dimensional attack-movement path and gives both features one safe final
trigger gate. Each link is optional and discovered at module load; ordinary
turret behavior is unchanged when either filter module is absent.

## Runtime behaviour

- The whole turret SOD rotates as one rigid object. Separate yaw-base and
  pitch-barrel articulation is not part of this first version.
- Automatic firing and explicit retargeting both feed the linked object's
  visual yaw/pitch tracking.
- The turret uses native weapon and damage behaviour. Destroying it removes
  that mount for the rest of the current session.
- Team and race changes are copied from the parent, including capture.
- Destroying the parent expires its linked turrets.
- Save/load uses deterministic child labels to reconnect saved turret objects
  without spawning duplicates.
- A turret destroyed before saving currently returns when that save is loaded,
  because destroyed-mount state is not yet serialized separately.
- Each mount is a full engine object. Ships with very large turret counts have
  a real simulation and rendering cost.
- Multiplayer determinism and late-game save compatibility have not yet been
  validated in game.

Use `A2FOExtensions.log` to diagnose incomplete pairs, missing hardpoints,
missing turret ODFs, or a referenced ODF which did not use the semantic
`turret` classlabel.
