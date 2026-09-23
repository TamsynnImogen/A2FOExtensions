/*
 * File: core/game_monitor.hpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Win32 monitor discovery, persistence, and position-only placement.
 */

#pragma once

#include "game_monitor_policy.hpp"

#include <windows.h>

#include <string>
#include <vector>

namespace a2fo {

std::vector<GameMonitorDescriptor> enumerate_game_monitors();

bool read_configured_game_monitor(
    const std::string& data_root, std::string& device_name);
bool write_configured_game_monitor(
    const std::string& data_root, const std::string& device_name);

GameMonitorResolution resolve_configured_game_monitor(
    const std::string& data_root,
    const std::vector<GameMonitorDescriptor>& monitors,
    int native_adapter_fallback,
    std::string* configured_device = nullptr);

// Move an existing top-level window without changing its dimensions, style,
// z-order, activation, or the game's selected display mode.
bool position_window_on_game_monitor(
    HWND window, const GameMonitorDescriptor& monitor);
bool is_window_on_game_monitor(
    HWND window, const GameMonitorDescriptor& monitor);

}  // namespace a2fo
