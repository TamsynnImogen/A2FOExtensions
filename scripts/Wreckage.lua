-- Replaces destroyed craft when their ODF opts into the feature:
--
--     wreckage = "my_wreckage_odf"
--     wreckageChance = 50
--
-- wreckageChance is a percentage from 0 to 100 and defaults to 100.
a2fo.require_api(1, 1)

a2fo.on_object_destroyed({"wreckage", "wreckageChance"}, function(event)
    local wreckage = event.odf:get_string("wreckage")
    if wreckage == nil or wreckage == "" then
        return nil
    end

    local chance_text = event.odf:get_string("wreckageChance", "100")
    local chance = tonumber(chance_text)
    if chance == nil or chance < 0 or chance > 100 then
        a2fo.log("Ignoring invalid wreckageChance on " ..
                 event.odf:get_string("basename", "<unknown>"))
        return nil
    end
    if not event:roll_percent(chance) then
        return nil
    end

    return {
        odf = wreckage,
        inherit_position = true,
        inherit_rotation = true,
        owner = "neutral"
    }
end)

a2fo.log("Wreckage replacement script initialized")
