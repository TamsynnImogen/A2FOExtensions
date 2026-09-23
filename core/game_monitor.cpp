/*
 * File: core/game_monitor.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Win32 monitor discovery, persistence, and position-only placement.
 */

#include "game_monitor.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace a2fo {
namespace {

constexpr char kDisplaySection[] = "Display";
constexpr char kGameMonitorKey[] = "GameMonitorDevice";
constexpr char kUnsetMonitor[] = "<A2FO-unset>";

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

std::string renderer_ini_path(const std::string& data_root) {
    return join_path(data_root, "A2FORenderer.ini");
}

struct MonitorInfoSearch {
    std::string device_name;
    HMONITOR handle = nullptr;
    MONITORINFOEXA info{};
};

BOOL CALLBACK find_monitor_info(HMONITOR handle, HDC, LPRECT,
                                LPARAM parameter) {
    auto* search = reinterpret_cast<MonitorInfoSearch*>(parameter);
    if (!search) return FALSE;
    MONITORINFOEXA info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(handle, &info)) return TRUE;
    if (_stricmp(info.szDevice, search->device_name.c_str()) != 0) {
        return TRUE;
    }
    search->handle = handle;
    search->info = info;
    return FALSE;
}

bool get_monitor_info(const std::string& device_name,
                      HMONITOR& handle, MONITORINFOEXA& info) {
    MonitorInfoSearch search;
    search.device_name = device_name;
    EnumDisplayMonitors(nullptr, nullptr, find_monitor_info,
                        reinterpret_cast<LPARAM>(&search));
    if (!search.handle) return false;
    handle = search.handle;
    info = search.info;
    return true;
}

std::string friendly_monitor_name(const DISPLAY_DEVICEA& adapter) {
    DISPLAY_DEVICEA monitor{};
    monitor.cb = sizeof(monitor);
    for (DWORD index = 0;
         EnumDisplayDevicesA(adapter.DeviceName, index, &monitor, 0);
         ++index) {
        if (monitor.DeviceString[0] != '\0' &&
            (monitor.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) == 0) {
            return monitor.DeviceString;
        }
        monitor = DISPLAY_DEVICEA{};
        monitor.cb = sizeof(monitor);
    }
    return adapter.DeviceString;
}

BOOL CALLBACK collect_monitor_fallback(HMONITOR handle, HDC, LPRECT,
                                       LPARAM parameter) {
    auto* monitors = reinterpret_cast<std::vector<GameMonitorDescriptor>*>(
        parameter);
    if (!monitors) return FALSE;
    MONITORINFOEXA info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(handle, &info)) return TRUE;

    GameMonitorDescriptor monitor;
    monitor.device_name = info.szDevice;
    monitor.friendly_name = info.szDevice;
    monitor.adapter_index = static_cast<int>(monitors->size());
    monitor.left = info.rcMonitor.left;
    monitor.top = info.rcMonitor.top;
    monitor.width = info.rcMonitor.right - info.rcMonitor.left;
    monitor.height = info.rcMonitor.bottom - info.rcMonitor.top;
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    monitors->push_back(std::move(monitor));
    return TRUE;
}

}  // namespace

std::vector<GameMonitorDescriptor> enumerate_game_monitors() {
    std::vector<GameMonitorDescriptor> monitors;
    int adapter_index = 0;
    for (DWORD device_index = 0; device_index < 64; ++device_index) {
        DISPLAY_DEVICEA adapter{};
        adapter.cb = sizeof(adapter);
        if (!EnumDisplayDevicesA(nullptr, device_index, &adapter, 0)) break;
        if ((adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0 ||
            (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0 ||
            adapter.DeviceName[0] == '\0') {
            continue;
        }

        GameMonitorDescriptor monitor;
        monitor.device_name = adapter.DeviceName;
        monitor.friendly_name = friendly_monitor_name(adapter);
        monitor.adapter_index = adapter_index++;
        monitor.primary =
            (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;

        HMONITOR handle = nullptr;
        MONITORINFOEXA info{};
        if (get_monitor_info(monitor.device_name, handle, info)) {
            monitor.left = info.rcMonitor.left;
            monitor.top = info.rcMonitor.top;
            monitor.width = info.rcMonitor.right - info.rcMonitor.left;
            monitor.height = info.rcMonitor.bottom - info.rcMonitor.top;
            monitor.primary = monitor.primary ||
                (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        } else {
            DEVMODEA mode{};
            mode.dmSize = sizeof(mode);
            if (EnumDisplaySettingsExA(
                    monitor.device_name.c_str(), ENUM_CURRENT_SETTINGS,
                    &mode, 0)) {
                monitor.left = mode.dmPosition.x;
                monitor.top = mode.dmPosition.y;
                monitor.width = static_cast<int>(mode.dmPelsWidth);
                monitor.height = static_cast<int>(mode.dmPelsHeight);
            }
        }
        monitors.push_back(std::move(monitor));
    }

    // Wine and unusual virtual-display drivers can expose monitors without
    // useful EnumDisplayDevices flags. Retain a conservative Win32 fallback.
    if (monitors.empty()) {
        EnumDisplayMonitors(
            nullptr, nullptr, collect_monitor_fallback,
            reinterpret_cast<LPARAM>(&monitors));
    }
    return monitors;
}

bool read_configured_game_monitor(
    const std::string& data_root, std::string& device_name) {
    device_name.clear();
    if (data_root.empty()) return false;
    char value[128]{};
    GetPrivateProfileStringA(
        kDisplaySection, kGameMonitorKey, kUnsetMonitor,
        value, sizeof(value), renderer_ini_path(data_root).c_str());
    if (std::strcmp(value, kUnsetMonitor) == 0 || value[0] == '\0') {
        return false;
    }
    device_name = value;
    return true;
}

bool write_configured_game_monitor(
    const std::string& data_root, const std::string& device_name) {
    if (data_root.empty() || device_name.empty()) return false;
    return WritePrivateProfileStringA(
               kDisplaySection, kGameMonitorKey, device_name.c_str(),
               renderer_ini_path(data_root).c_str()) != FALSE;
}

GameMonitorResolution resolve_configured_game_monitor(
    const std::string& data_root,
    const std::vector<GameMonitorDescriptor>& monitors,
    int native_adapter_fallback,
    std::string* configured_device) {
    std::string device_name;
    read_configured_game_monitor(data_root, device_name);
    if (configured_device) *configured_device = device_name;
    return resolve_game_monitor(
        monitors, device_name, native_adapter_fallback);
}

bool position_window_on_game_monitor(
    HWND window, const GameMonitorDescriptor& monitor) {
    if (!window || !IsWindow(window)) return false;

    HMONITOR handle = nullptr;
    MONITORINFOEXA info{};
    if (!get_monitor_info(monitor.device_name, handle, info)) return false;

    RECT window_rect{};
    if (!GetWindowRect(window, &window_rect)) return false;
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    if (width <= 0 || height <= 0) return false;

    const LONG_PTR style = GetWindowLongPtrA(window, GWL_STYLE);
    const bool framed = (style & (WS_CAPTION | WS_THICKFRAME)) != 0;
    const RECT& target = framed ? info.rcWork : info.rcMonitor;
    int left = target.left;
    int top = target.top;
    if (framed) {
        const int horizontal_space =
            static_cast<int>(target.right - target.left) - width;
        const int vertical_space =
            static_cast<int>(target.bottom - target.top) - height;
        left += std::max(0, horizontal_space / 2);
        top += std::max(0, vertical_space / 2);
    }

    return SetWindowPos(
               window, nullptr, left, top, 0, 0,
               SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOOWNERZORDER |
                   SWP_NOZORDER) != FALSE;
}

bool is_window_on_game_monitor(
    HWND window, const GameMonitorDescriptor& monitor) {
    if (!window || !IsWindow(window)) return false;
    HMONITOR target = nullptr;
    MONITORINFOEXA info{};
    if (!get_monitor_info(monitor.device_name, target, info)) return false;
    return MonitorFromWindow(window, MONITOR_DEFAULTTONULL) == target;
}

}  // namespace a2fo
