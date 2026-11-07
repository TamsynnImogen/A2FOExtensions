#pragma once
#include <windows.h>
#include <string>

namespace a2fo::renderer {
// Kept separate from D3D8 headers (legacy and modern SDK enum conflicts).
HRESULT run_d3d11_window_probe(HWND window, UINT width, UINT height,
                             bool debug_layer,
                             void (*log)(const std::string&));
}
