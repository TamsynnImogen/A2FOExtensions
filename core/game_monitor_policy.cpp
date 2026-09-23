/*
 * File: core/game_monitor_policy.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Platform-neutral game-monitor selection and labelling policy.
 */

#include "game_monitor_policy.hpp"

#include <cctype>

namespace a2fo {
namespace {

bool same_device_name(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const unsigned char left_character =
            static_cast<unsigned char>(left[index]);
        const unsigned char right_character =
            static_cast<unsigned char>(right[index]);
        if (std::tolower(left_character) != std::tolower(right_character)) {
            return false;
        }
    }
    return true;
}

int primary_or_first(const std::vector<GameMonitorDescriptor>& monitors) {
    for (std::size_t index = 0; index < monitors.size(); ++index) {
        if (monitors[index].primary) return static_cast<int>(index);
    }
    return monitors.empty() ? -1 : 0;
}

}  // namespace

GameMonitorResolution resolve_game_monitor(
    const std::vector<GameMonitorDescriptor>& monitors,
    const std::string& configured_device,
    int native_adapter_fallback) {
    GameMonitorResolution resolution;
    resolution.configured = !configured_device.empty();

    if (resolution.configured) {
        for (std::size_t index = 0; index < monitors.size(); ++index) {
            if (same_device_name(
                    monitors[index].device_name, configured_device)) {
                resolution.monitor_index = static_cast<int>(index);
                resolution.configured_device_present = true;
                return resolution;
            }
        }
        resolution.monitor_index = primary_or_first(monitors);
        return resolution;
    }

    for (std::size_t index = 0; index < monitors.size(); ++index) {
        if (monitors[index].adapter_index == native_adapter_fallback) {
            resolution.monitor_index = static_cast<int>(index);
            return resolution;
        }
    }
    resolution.monitor_index = primary_or_first(monitors);
    return resolution;
}

std::string game_monitor_label(const GameMonitorDescriptor& monitor) {
    std::string label = "Display " +
        std::to_string(monitor.adapter_index + 1) + " - ";
    label += monitor.friendly_name.empty()
        ? monitor.device_name
        : monitor.friendly_name;
    if (monitor.width > 0 && monitor.height > 0) {
        label += " - " + std::to_string(monitor.width) + "x" +
                 std::to_string(monitor.height);
    }
    if (monitor.primary) label += " (Primary)";
    return label;
}

bool is_legacy_monitor_ordinal_display_width(int display_width) {
    return display_width > 0 && display_width < 64;
}

}  // namespace a2fo
