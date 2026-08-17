# A2FONormalWeaponTech

`A2FONormalWeaponTech.dll` makes ordinary weapons obey the owning team's
active Fleet Operations technology tree.

Normal cannon, phaser, pulse, and torpedo weapon ODFs may be listed in the
usual `.tt` file using the existing syntax:

```text
fbphas.odf 0
fbpulse.odf 1 fresearch.odf
fblockedweapon.odf -1
```

- An unlisted normal weapon defaults to `0` and remains available.
- A listed weapon uses Fleet Operations' native recursive prerequisite
  evaluator, including the normal meanings of `0`, positive requirement
  counts, `-1`, and `-2`.
- The current owner's team is checked for every trigger, so research
  completion and captured ships take effect without recreating the weapon.
- `special = 1` weapons are left to Fleet Operations' existing special-weapon
  technology handling.
- Invalid or unavailable runtime state fails open so the extension cannot
  disable every weapon merely because a supported structure is absent.

The module registers with the core's shared `Weapon::Trigger(GameObject)`
dispatcher and does not require `A2FOTurrets.dll`. The exported
`A2FONormalWeaponTech_AllowWeaponTrigger` entry remains available for older
native clients.
