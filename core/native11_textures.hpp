#pragma once
#include <windows.h>
#include "../renderer/native11/texture_store.hpp"
namespace a2fo::native11_textures {
using Log=void(*)(const char*);
// Startup-only, default-off resolver observation. No image API is called.
bool initialize(HMODULE,bool requested,Log) noexcept;
using OpenReadOwnerCheck=bool(*)(HMODULE) noexcept;
bool initialize_tga(HMODULE,bool requested,Log,OpenReadOwnerCheck=nullptr,HMODULE fleet_ops=nullptr) noexcept;
void disable() noexcept;
// Before native texture destruction; rejects pending late publication too.
bool retire_texture(std::uint32_t texture) noexcept;
std::shared_ptr<const native11::TextureAsset> current(std::uint32_t texture) noexcept;
// Exact native logical names only. Unsupported/ambiguous sources fail closed.
bool collect(std::vector<native11::SnapshotTexture>&,std::uint64_t& generation,std::string& error) noexcept;
bool collect_required(const std::vector<std::string>&,
    std::vector<native11::SnapshotTexture>&,std::uint64_t& generation,std::string& error) noexcept;
}
extern "C" std::uintptr_t a2fo_native11_texture_resolver_original;
extern "C" void a2fo_native11_texture_parameters_tap();
extern "C" void a2fo_native11_texture_pixels_tap();
extern "C" void a2fo_native11_observe_texture_path(void* output_slot,const char* requested,
    void* caller_frame,std::uint32_t route) noexcept;
extern "C" std::uintptr_t a2fo_native11_tga_open_original;
extern "C" void a2fo_native11_tga_open_tap();
extern "C" void a2fo_native11_observe_tga_open(std::uint32_t result,void* stream,
    const char* requested,std::uint32_t buffered,void* texture,std::uint32_t level) noexcept;
