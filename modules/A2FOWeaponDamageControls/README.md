# Weapon damage controls

`A2FOWeaponDamageControls.dll` adds optional shield/hull controls to ordinary
weapon ODFs:

```cpp
canDamageShields = true
canDamageHull = true
shieldDamageModifier = 1.0
hullDamageModifier = 1.0
```

The boolean commands default to `true` and both modifiers default to `1.0`, so
existing weapons retain their normal behaviour.
The commands belong in the **weapon ODF** (`fbphas.odf`), not the referenced
ordnance ODF (`fbphaso.odf`).

| Settings | Result |
| --- | --- |
| both `true` | normal shield damage and hull spillover |
| shields `false`, hull `true` | shields block the complete hit; damage hull normally once shields are down |
| shields `true`, hull `false` | damage shields and discard hull spillover |
| both `false` | apply no primary shield or hull damage |

The modifiers multiply the resolved per-hit damage for their channel. For
example:

```cpp
shieldDamageModifier = 2.0
hullDamageModifier = 0.5
```

Like Armada's `hitChance` and `damageBase` lookups, either modifier can also
provide values for individual target-unit ODFs:

```cpp
shieldDamageModifier = 1.0 "fed_sovereign.odf" 0.5 "bcruise1.odf" 2.0
hullDamageModifier = 1.0 "fed_sovereign.odf" 1.5 "bcruise1.odf" 0.25
```

The first value is the fallback. Each quoted ODF filename and following value
is an override used when that unit class is the target. The module uses
Armada's native project-ID lookup, so the filename matching behaves exactly
like the stock target tables rather than using a separate string matcher.
A child weapon ODF inherits the complete lookup when it does not redeclare
that modifier.

The weapon deals twice its ordinary damage while hitting shields and half its
ordinary damage against exposed hull. On a shield-breaking hit, shield
consumption uses the shield modifier and resulting spillover is corrected to
the hull modifier. A modifier of `0` removes damage from that channel. Values
must be finite and zero or greater; each invalid fallback or target value is
replaced with `1.0`.

The module reads the target's current shield strength for each hit and adds
native DamageInfo flags on a private copy. A weapon with
`canDamageShields = false` therefore cannot reduce or pass through positive
shield strength. Existing ordnance flags such as `ignoreShield` are preserved
rather than cleared, and special weapon effects remain the responsibility of
their original ordnance implementation.
