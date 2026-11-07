#pragma once
#include <windows.h>
#include <string>

namespace a2fo::renderer {
using RendererLog = void (*)(const std::string&);
// Optional, early PE import interception. No graphics DLL loading or GPU work
// occurs here. Legacy observation keeps provider resources authoritative.
// An explicitly opted-in child can instead create the experimental D3D11
// game device. Disabled by default in A2FORenderer.ini [D3D8Compat].
// Startup-only: call once under serialized core attach, before the game entry
// point. The observer pins its owning module; hot unload is unsupported.
bool install_d3d8_observer(HMODULE executable, HMODULE fleet_ops,
                          const std::string& data_root, RendererLog log);
// Startup selection, available before optional material hooks are installed.
bool d3d11_game_test_requested() noexcept;
// Writes cumulative method/signature counts. Call only outside DllMain.
void flush_d3d8_observer() noexcept;
}
