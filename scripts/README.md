# Lua scripts

The built-in recursive ODF and queue features live in
`A2FOFeaturePack.dll`. The Armada 1-specific `wingman -> craft` alias lives in
`A1Compat.dll`, while HybridBuild owns its Evolver cocoon policy. These are
native engine extensions and are not represented by token Lua files.

`Wreckage.lua` is a production example with actual gameplay logic. It reads
`wreckage` and `wreckageChance` from a destroyed craft's ODF, makes a
deterministic chance roll, and asks the native bridge to construct the chosen
neutral wreckage at the old transform.

`UpgradePods.lua` owns the startup-only upgrade-pod policy and currently sets
the maximum to tier 6. A mod can replace it by shipping a script with the same
basename in its own `scripts` folder.

Modders may add or replace scripts when they need conditional logic through
the callback API:

```lua
a2fo.on_classlabel(function(classlabel, odf)
    return replacement_or_nil
end)

a2fo.on_evolver_cocoon(function(odf)
    return sod_name_or_nil
end)

a2fo.require_api(1, 2)
a2fo.configure_upgrade_pods({ maximum_tier = 6 })
a2fo.on_object_destroyed({"fieldName"}, function(event)
    return replacement_table_or_nil
end)

odf:get_string(command, optional_default)
```

A parent or active mod can replace a shared script by using the same basename
in its own `scripts` folder. Only the highest-precedence copy executes.

See `../docs/lua-api.md`, `Wreckage.lua`, and
`scripts/examples/ModSpecificRules.lua`. The latter example is kept in a
subdirectory so it is not loaded automatically.
