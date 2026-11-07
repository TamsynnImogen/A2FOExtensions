#pragma once

#include "texture.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Decodes only mip zero into tightly packed, top-row-first BGRA8 bytes.
// All authored levels are validated first. sRGB channel bytes stay encoded;
// callers using a linear UNORM PrimitiveTexture must account for that limit.
// Opaque alpha becomes 255; unknown/straight alpha is preserved. The optional
// key clears alpha only for exact encoded RGB (255,0,255) after decoding.
// On failure, out is unchanged. No GPU or file-format objects are involved.
bool texture_base_to_bgra(TextureView view, bool magenta_key,
                          std::vector<std::uint8_t>& out,
                          std::string& error) noexcept;
} // namespace a2fo::native11
