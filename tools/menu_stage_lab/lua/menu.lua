-- Engine-free menu component probe. View names are host-whitelisted fixtures.
Menu.Camera.gotoView("opening")

local visit = Timer("visit_hangar", 4.0, Timer.State.Started)
visit:hook("visit_hangar_hook", nil, function(timerObject, hook)
    Menu.Camera.panTo("hangar", 2.0)
    timerObject:stop()
end, nil)
