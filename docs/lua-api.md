# Lua API

Scripts are selected from `Data\scripts`, parent mods, and the active mod.
Matching basenames are overlaid from lowest to highest precedence before any
script executes. Selected files execute once at deferred startup in
case-insensitive filename order.

## General values

```lua
a2fo.api_version       -- major Lua API version (1)
a2fo.api_revision      -- compatible appended revision (2)
a2fo.lua_version       -- embedded Lua version string
a2fo.extension_roots   -- low-to-high-precedence root list
a2fo.log(message)      -- writes a prefixed line to A2FOExtensions.log
a2fo.require_api(1, 2) -- stop this script if the host is too old
a2fo.has_capability("declared_destroyed_odf_fields")
a2fo.has_capability("configurable_upgrade_pods")
```

Scripts should call `a2fo.require_api` before registering callbacks that depend
on a revision. A script's registrations are transactional: if its startup chunk
errors, all callbacks and ownership claims made by that script are rolled back
before the next script runs.

## Upgrade-pod policy

```lua
a2fo.require_api(1, 2)
a2fo.configure_upgrade_pods({ maximum_tier = 6 })
```

One selected startup script may own this policy. `maximum_tier` must be an
integer from 3 through the native hard ceiling of 16. With no policy the
vanilla maximum remains 3. A failed script relinquishes the ownership claim
and restores the previous value transactionally. Because the policy affects
simulation state, every multiplayer peer must use the same script.

The function exposes configuration only; checked native hooks in
`A2FOFeaturePack.dll` implement the engine behaviour. See
[`upgrade-pods.md`](upgrade-pods.md).

## Classlabel callback

```lua
a2fo.on_classlabel(function(classlabel, odf)
    if classlabel == "wingman" then
        return "craft"
    end
    return nil
end)
```

Every registered classlabel callback runs in deterministic script order. The
current value is passed to the next callback. Return a 1-63 character
identifier to replace it, or `nil` to leave it unchanged.

## Evolver cocoon callback

```lua
a2fo.on_evolver_cocoon(function(odf)
    return odf:get_string("cocoon")
end)
```

One selected script may own this callback. Return a SOD name, with or without
`.sod`, or `nil` to retain Fleet Operations' original choice. The native bridge
owns class lifetime association, SOD loading, caching, and fallback selection.

## Destroyed-object callback

```lua
a2fo.require_api(1, 1)

a2fo.on_object_destroyed({"wreckage", "wreckageChance"}, function(event)
    local wreckage = event.odf:get_string("wreckage")
    if wreckage == nil or wreckage == "" then
        return nil
    end

    local chance = tonumber(event.odf:get_string("wreckageChance", "100"))
    if chance == nil or chance < 0 or chance > 100 or
       not event:roll_percent(chance) then
        return nil
    end

    return {
        odf = wreckage,
        inherit_position = true,
        inherit_rotation = true,
        owner = "neutral"
    }
end)
```

The corresponding source ODF opts in with ordinary commands:

```text
wreckage = "my_ship_wreck"
wreckageChance = 50
```

`wreckageChance` is a percentage from 0 through 100. It is read by the Lua
script rather than by a hard-coded wreckage feature. `event:roll_percent()` is
deterministic for the destroyed object, so the result does not depend on Lua's
global random state and remains suitable for synchronized games.

Return `nil` to do nothing, or return one replacement table. `odf` must be a
basename containing only letters, digits, `_`, `-`, or `.`. The inheritance
fields default to `true`. `owner` may be `"neutral"` or `"original"` and
defaults to `"neutral"`. Callbacks run in script order; the first non-`nil`
replacement claims the event.

Field names are validated, normalized case-insensitively, deduplicated, and
copied during script startup. `basename` is always present and need not be
declared. The old one-argument form remains compatible, implicitly requests
`wreckage` and `wreckageChance`, and logs a deprecation notice.

The native bridge supplies the checked Craft destruction hook, copies the
source transform before Armada destroys it, finds and constructs the selected
ODF, and publishes the new object through `AiMission::AddObject`. Lua receives
no engine pointer.

## Temporary ODF view

```lua
local value = odf:get_string("command")
local value_or_default = odf:get_string("command", "default")
```

A missing command returns `nil` unless a default was supplied. The view is
valid only within the callback that received it. Saving it in a global or
closure and using it during a later callback raises an error. This prevents
Lua from retaining an Armada ParameterDB pointer after its lifetime ends.
Destroyed-object callbacks receive a safe snapshot containing `basename` and
the union of fields explicitly requested by active native and Lua handlers;
other callbacks read the live ParameterDB through the same bounded interface.

Callbacks share the host's 32 MiB memory limit and have an instruction budget.
An error or runaway callback is logged with a traceback, disabled for the
remainder of the process, and falls back safely to native behaviour.

For destruction events, native module handlers run first in deterministic
registration order, followed by Lua callbacks in deterministic script order.
The first valid replacement claims the event.

The API intentionally exposes semantic operations rather than raw addresses or
engine objects. New kinds of engine behaviour still require a checked native
hook/action to be added to the core once; modders can then implement variations
of that behaviour in Lua without shipping another DLL.
