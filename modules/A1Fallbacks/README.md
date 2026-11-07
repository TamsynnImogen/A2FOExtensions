# A1Fallbacks built-in module

`A1Fallbacks` supplies missing-only GUI imagery for object wireframes. It is an
opt-in module separate from `A1Compat` and is built into `A2FOExtensions.dll`.
Keep the original `A1Fallbacks` name in module selections.

For both the selected-object display and native build-queue icons, the module
keeps any ordinary `w1` through `w5` (or single `_s`) wireframe supplied by the
active mod. Only when the entire native wireframe set is absent does it try:

1. the object's normal build-button sprite, `b_<object basename>`;
2. the current owner's faction emblem, `<faction name>_icon`.

This means captured objects use their current owner's faction icon if both of
the object's own image sets are unavailable. Missing build buttons and missing
faction icons remain blank; the module never replaces a valid wireframe.
While disabled, the hook forwards the native resolver result unchanged.

Select the module in a mod's `info.ini`:

```ini
[modules]
active0="A1Fallbacks"
```

The core prepares the module once unless it is rejected, including when it is
initially off. It is available only when its own executable signature check
and native hook setup succeed. A stale standalone `A1Fallbacks.dll` is ignored
during discovery. The standalone target remains available for SDK compatibility;
the release no longer needs this DLL.
