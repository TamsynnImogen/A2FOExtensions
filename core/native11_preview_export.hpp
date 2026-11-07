#pragma once
#include "../renderer/native11/preview_export.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace a2fo::native11_preview_export {
using Log=void(*)(const char*);
using Readback=bool(*)(void*,std::vector<std::uint8_t>&,std::uint32_t&,std::uint32_t&,std::string&);
using Current=bool(*)(void*) noexcept;
// CPU/filesystem setup only. Production output is beside the executable in
// Native11Preview. No window or input device is opened by this diagnostic.
bool configure(bool enabled,std::uint32_t interval_ms,Log) noexcept;
// Explicit directory for isolated fixtures; never a renderer INI path option.
bool configure_at(const std::string& directory,std::uint32_t interval_ms,Log,std::string&) noexcept;
bool requested() noexcept;
void sample(native11::PreviewExportStatus,Readback,Current,void*,std::uint64_t now_ms) noexcept;
bool stop() noexcept;
}
