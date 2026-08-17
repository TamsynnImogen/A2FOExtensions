# Native wreckage replacements

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
