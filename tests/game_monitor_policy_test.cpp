#include "game_monitor_policy.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

a2fo::GameMonitorDescriptor monitor(
    const char* device, const char* name, int adapter,
    int width, int height, bool primary) {
    a2fo::GameMonitorDescriptor result;
    result.device_name = device;
    result.friendly_name = name;
    result.adapter_index = adapter;
    result.width = width;
    result.height = height;
    result.primary = primary;
    return result;
}

}  // namespace

int main() {
    const std::vector<a2fo::GameMonitorDescriptor> monitors{
        monitor("\\\\.\\DISPLAY1", "Laptop Panel", 0, 1920, 1080, true),
        monitor("\\\\.\\DISPLAY2", "External Monitor", 1, 2560, 1440,
                false),
    };

    auto selected = a2fo::resolve_game_monitor(
        monitors, "\\\\.\\display2", 0);
    assert(selected.configured);
    assert(selected.configured_device_present);
    assert(selected.monitor_index == 1);

    selected = a2fo::resolve_game_monitor(
        monitors, "\\\\.\\DISPLAY9", 1);
    assert(selected.configured);
    assert(!selected.configured_device_present);
    assert(selected.monitor_index == 0);

    selected = a2fo::resolve_game_monitor(monitors, "", 1);
    assert(!selected.configured);
    assert(selected.monitor_index == 1);

    selected = a2fo::resolve_game_monitor(monitors, "", 99);
    assert(selected.monitor_index == 0);

    selected = a2fo::resolve_game_monitor({}, "\\\\.\\DISPLAY2", 0);
    assert(selected.configured);
    assert(selected.monitor_index == -1);

    assert(a2fo::game_monitor_label(monitors[0]) ==
           "Display 1 - Laptop Panel - 1920x1080 (Primary)");
    assert(a2fo::game_monitor_label(monitors[1]) ==
           "Display 2 - External Monitor - 2560x1440");
    assert(!a2fo::is_legacy_monitor_ordinal_display_width(0));
    assert(a2fo::is_legacy_monitor_ordinal_display_width(1));
    assert(a2fo::is_legacy_monitor_ordinal_display_width(63));
    assert(!a2fo::is_legacy_monitor_ordinal_display_width(64));
    assert(!a2fo::is_legacy_monitor_ordinal_display_width(640));
    return 0;
}
