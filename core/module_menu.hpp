/*
 * File: core/module_menu.hpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Fleet Operations Mods-screen module-selection integration.
 */

#pragma once

#include <windows.h>

#include <string>

namespace a2fo {

bool install_module_menu(HMODULE fleet_ops, const std::string& data_root,
                         void (*log_line)(const std::string&));

}  // namespace a2fo
