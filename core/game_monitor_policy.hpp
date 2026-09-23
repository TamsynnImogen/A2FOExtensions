/*
 * File: core/game_monitor_policy.hpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Platform-neutral game-monitor selection and labelling policy.
 */

#pragma once

#include <string>
#include <vector>

namespace a2fo {

struct GameMonitorDescriptor {
    std::string device_name;
    std::string friendly_name;
    int adapter_index = -1;
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    bool primary = false;
};

struct GameMonitorResolution {
    int monitor_index = -1;
    bool configured = false;
    bool configured_device_present = false;
};

// A saved device name is authoritative because numeric display ordinals can
// change after hot-plugging. With no A2FO setting, use the supplied UI choice
// as an enumeration fallback. If either choice is unavailable, use the current
// primary monitor and finally the first attached monitor.
GameMonitorResolution resolve_game_monitor(
    const std::vector<GameMonitorDescriptor>& monitors,
    const std::string& configured_device,
    int native_adapter_fallback);

std::string game_monitor_label(const GameMonitorDescriptor& monitor);

// A short-lived monitor implementation accidentally stored its zero-based
// monitor ordinal in FoSettings' display-width field. Zero is a valid native
// automatic-mode value, but positive ordinals from that build are not valid
// display widths and need a one-time reset.
bool is_legacy_monitor_ordinal_display_width(int display_width);

}  // namespace a2fo
