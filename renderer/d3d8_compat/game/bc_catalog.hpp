#pragma once

#include "../../textures/dds.hpp"

#include <cstddef>
#include <string>

namespace a2fo::renderer::game {

// Loads catalog v2 from modroot/.a2fo-dx11/catalog.txt before texture loading.
// Repeated calls for the same normalized root succeed without reloading.
// This private transport is never a native D3D8 resource.
bool load_bc_catalog(const std::string& modroot,
                     void (*log)(const std::string&), std::string& error);

// Loads only a listed canonical DDS basename from the catalog root. An
// unlisted name is a clean miss; corrupt listed assets set error. Outputs are
// unchanged on failure.
bool load_bc_asset(const char* filename, textures::CompressedImage& out,
                   bool& normal, std::string& asset, std::string& error);

// Returns an empty string unless the requested flat DDS basename is listed in
// the loaded catalog. The caller may pass the result to its original fopen.
std::string bc_transport_path(const char* requestedfilename);

// Matches exact bytes at any supplied source mip level, then returns only the
// requested remaining chain with rebased offsets. No mip data is generated.
// On failure, output and metadata are untouched; no match clears error, while
// a corrupt/ambiguous candidate sets it.
bool match_bc_texture(UINT width, UINT height, UINT mips, const void* base,
                      std::size_t size, textures::CompressedImage& out,
                      bool& normal, std::string& asset, std::string& error);

} // namespace a2fo::renderer::game
