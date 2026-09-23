/*
 * File: core/renderer_options.hpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Fleet Operations Graphics Options renderer and monitor selection.
 */

#pragma once

#include <windows.h>

#include <string>

namespace a2fo {

// Adds restart-applied System Direct3D 9 / DXVK and Game Monitor selectors to
// Fleet Ops' native Graphics Options form. Both selections are installation-
// wide because the renderer and primary display are chosen before a mod loads.
bool install_renderer_options(HMODULE fleet_ops, const std::string& data_root,
                              void (*log_line)(const std::string&));

// Live map-effect switches persisted in Data\A2FORenderer.ini. They default
// on, and are exposed beside Fleet Operations' native Bump Mapping option.
bool renderer_emissive_maps_enabled() noexcept;
bool renderer_specular_maps_enabled() noexcept;

}  // namespace a2fo
