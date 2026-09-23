# A1Fallbacks native module

`A1Fallbacks.dll` supplies missing-only GUI imagery for object wireframes.
It is a separate opt-in module from `A1Compat.dll`.

For both the selected-object display and native build-queue icons, the module
keeps any ordinary `w1` through `w5` (or single `_s`) wireframe supplied by the
active mod. Only when the entire native wireframe set is absent does it try:

1. the object's normal build-button sprite, `b_<object basename>`;
2. the current owner's faction emblem, `<faction name>_icon`.

This means captured objects use their current owner's faction icon if both of
the object's own image sets are unavailable. Missing build buttons and missing
faction icons remain blank; the module never replaces a valid wireframe.

Select the centrally installed module in a mod's `info.ini`:

```ini
[modules]
active0="A1Fallbacks"
```
