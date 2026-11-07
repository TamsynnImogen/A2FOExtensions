# Native wreckage replacements

This feature is now built into `A2FOExtensions.dll`. Keep using `A2FOWreckage`
in the Modules menu and `info.ini`; its standalone DLL is unnecessary and is
ignored by the new core. See [built-in module behaviour](../../docs/builtin-modules-stage1.md).

`A2FOWreckage.dll` optionally creates a neutral replacement object when a
Craft is destroyed:

```cpp
wreckage = "fed_sovereign_wreck"
wreckageChance = 50
```

`wreckageChance` is a percentage from `0` through `100` and defaults to
`100`. The replacement inherits the destroyed object's position and rotation.
The roll is derived from the core's deterministic destruction seed so all
players reach the same result in synchronized games. Missing `wreckage`, an
empty value, or an invalid chance leaves native destruction unchanged.
