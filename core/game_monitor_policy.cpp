#include "game_monitor_policy.hpp"
#include "../rust/ui_ffi.h"
#include <cctype>
#include <vector>
namespace a2fo {
GameMonitorResolution resolve_game_monitor(const std::vector<GameMonitorDescriptor> &monitors,
                                           const std::string &configured_device,
                                           int native_adapter_fallback) {
    auto fold = [](std::string value) {
        for (char &c : value)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return value;
    };
    std::vector<std::string> names;
    names.reserve(monitors.size());
    for (const auto &m : monitors)
        names.push_back(fold(m.device_name));
    const std::string configured = fold(configured_device);
    std::vector<A2foRsMonitor> input;
    input.reserve(monitors.size());
    for (std::size_t i = 0; i < monitors.size(); ++i) {
        const auto &m = monitors[i];
        input.push_back({reinterpret_cast<const uint8_t *>(names[i].data()), names[i].size(),
                         m.adapter_index, static_cast<uint8_t>(m.primary)});
    }
    const auto r = a2fo_rs_ui_monitor(input.data(), input.size(),
                                      reinterpret_cast<const uint8_t *>(configured.data()),
                                      configured.size(), native_adapter_fallback);
    return {r.index, r.configured != 0, r.present != 0};
}
std::string game_monitor_label(const GameMonitorDescriptor &monitor) {
    std::string label = "Display " + std::to_string(monitor.adapter_index + 1) + " - ";
    label += monitor.friendly_name.empty() ? monitor.device_name : monitor.friendly_name;
    if (monitor.width > 0 && monitor.height > 0)
        label += " - " + std::to_string(monitor.width) + "x" + std::to_string(monitor.height);
    if (monitor.primary)
        label += " (Primary)";
    return label;
}
bool is_legacy_monitor_ordinal_display_width(int width) {
    return a2fo_rs_ui_legacy_width(width) != 0;
}
} // namespace a2fo
