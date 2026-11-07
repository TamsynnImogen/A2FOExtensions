#pragma once
#include <windows.h>
// Internal built-in ownership check, never an SDK export or a hook installer.
bool a2fo_builtin_rgb_owns_open_read(HMODULE) noexcept;
