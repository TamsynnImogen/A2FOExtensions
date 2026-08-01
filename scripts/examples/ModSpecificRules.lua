-- Example only: copy this file directly into a mod's scripts folder to enable
-- it. It demonstrates logic that is meaningfully easier to customize in Lua
-- than in a native DLL.

local legacy_classlabels = {
    wingman = "craft",
}

a2fo.on_classlabel(function(classlabel, odf)
    -- Defines a new per-ODF compatibility command. It takes precedence over
    -- the shared legacy mapping table below.
    local explicit = odf:get_string("a2foClasslabel")
    if explicit ~= nil then
        return explicit
    end
    return legacy_classlabels[classlabel]
end)

a2fo.on_evolver_cocoon(function(odf)
    -- A mod may use the ordinary direct command...
    local direct = odf:get_string("cocoon")
    if direct ~= nil then
        return direct
    end

    -- ...or define two lightweight commands whose values are composed into a
    -- model name by Lua, without compiling another DLL:
    --
    --   cocoonFamily  = "8472_cocoon"
    --   cocoonVariant = "armoured"
    --
    -- This resolves to 8472_cocoon_armoured.sod.
    local family = odf:get_string("cocoonFamily")
    local variant = odf:get_string("cocoonVariant")
    if family ~= nil and variant ~= nil then
        return family .. "_" .. variant .. ".sod"
    end
    return nil
end)

a2fo.log("Mod-specific compatibility and cocoon rules initialized")
