#pragma once
#include <windows.h>
#include "../renderer/native11/model_store.hpp"
#include "../renderer/native11/asset_snapshot.hpp"
#include "../sdk/include/a2fo_native11_api.h"
namespace a2fo::native11_assets {
using Log=void(*)(const char*);
// Startup-only, default-off observation of the pinned model-load call. It
// changes no parser input/result and does not enable graphics submission.
bool initialize(HMODULE,bool requested,Log,HMODULE fleet_ops=nullptr) noexcept;
// Read-only ownership check for consumers of the completed load boundary.
bool owns_load_call(HMODULE) noexcept;
void disable() noexcept;
// Before native database destruction; releases only copied records/owners.
bool retire_model(std::uint32_t database) noexcept;
bool model_info(std::uint32_t database,native11::ModelAssetInfo&) noexcept;
std::shared_ptr<const native11::ModelAsset> pin_model(std::uint32_t database,std::uint64_t serial) noexcept;
// Current immutable owner under a copied database-name match; never guesses an
// ODF model or upgrades an old serial. Generation zero means unavailable.
std::shared_ptr<const native11::ModelAsset> pin_current_model(std::uint32_t,const char*) noexcept;
std::uint64_t generation() noexcept;
// Pins only versions already recorded in the frame. Missing/replaced versions
// fail closed; this does not resolve textures or approve parser semantics.
bool bind_frame(const native11::Frame&,
    std::shared_ptr<const native11::BoundCapture>&,std::string& error) noexcept;
// Sink is synchronous; the shared immutable payload remains alive after the
// store lock is released. Neither engine pointers nor allocators cross it.
bool read_model(std::uint32_t database,std::uint64_t serial,
                A2FO_N11_BytesSink,void*) noexcept;
}
extern "C" std::uintptr_t a2fo_native11_read_database_original;
extern "C" void a2fo_native11_database_read_tap();
extern "C" void a2fo_native11_observe_database(void* database,void* stream,
    std::uint32_t parser_scalar_bits,const char* name,std::uint32_t native_x87_control=0) noexcept;
