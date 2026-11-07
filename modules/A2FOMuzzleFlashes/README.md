# Weapon ODF muzzle flashes

This feature is built into `A2FOExtensions.dll`. Keep using
`A2FOMuzzleFlashes` in the Modules menu and weapon ODFs; its standalone DLL
remains available for compatibility testing and is not a release dependency.
See [built-in module behaviour](../../docs/builtin-modules-stage2.md).

Configure a flash in the **weapon ODF**, using a sprite identifier registered
by the mod's loaded `.spr` files:

```ini
muzzleFlashSprite = "my_pulse_muzzle"
muzzleFlashSize = 8.0
muzzleFlashDuration = 0.12
muzzleFlashColor = "1.0 0.65 0.25"
```

`muzzleFlashSprite` defaults to empty, leaving the flash disabled. Size and
duration must be positive (maximums `10000` and `10` respectively); RGB
channels range from `0` to `1`. Invalid settings disable that weapon's flash.
An explicit empty sprite disables an inherited flash. The sprite name is not a
texture filename, SOD, or ordnance ODF.

When the module is off, weapon metadata remains available for a later enable;
the live flag gates only flash emission. Original ordnance construction and
the existing A2FOAnimations firing notification continue independently of
that flag. Native particles expire normally. This feature does not change
projectiles, damage, ammunition, or shot timing.
