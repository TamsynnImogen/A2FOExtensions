/*
 * File: core/renderer_options.hpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Fleet Operations Graphics Options renderer selection.
 */

#pragma once

#include <windows.h>

#include <string>

namespace a2fo {

// Adds a restart-applied System Direct3D 9 / DXVK selector to Fleet Ops'
// native Graphics Options form. The selected backend is installation-wide,
// because d3d9.dll is chosen before any mod is loaded.
bool install_renderer_options(HMODULE fleet_ops, const std::string& data_root,
                              void (*log_line)(const std::string&));

// Live map-effect switches persisted in Data\A2FORenderer.ini. They default
// on, and are exposed beside Fleet Operations' native Bump Mapping option.
bool renderer_emissive_maps_enabled() noexcept;
bool renderer_specular_maps_enabled() noexcept;

}  // namespace a2fo
