#pragma once

#include "primitive_scene.hpp"
#include "../../sdk/include/a2fo_native11_primitives_api.h"
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {

// Synchronous API7 wire view. Descriptors and their payload owner stay alive
// until this object is destroyed. Move rebinds all descriptor-array pointers.
struct EncodedPrimitiveFrame {
    std::shared_ptr<const PrimitiveFrame> packet;
    std::vector<A2FO_N11_PrimitiveMesh> meshes;
    std::vector<A2FO_N11_PrimitiveTexture> textures;
    std::vector<A2FO_N11_PrimitiveDraw> draws;
    std::vector<A2FO_N11_Light> lights;
    A2FO_N11_PrimitiveFrame frame{};

    EncodedPrimitiveFrame()=default;
    EncodedPrimitiveFrame(const EncodedPrimitiveFrame&)=delete;
    EncodedPrimitiveFrame& operator=(const EncodedPrimitiveFrame&)=delete;
    EncodedPrimitiveFrame(EncodedPrimitiveFrame&&) noexcept;
    EncodedPrimitiveFrame& operator=(EncodedPrimitiveFrame&&) noexcept;
    void rebind() noexcept;
};

// Performs representation/budget checks, then publishes one owned wire packet.
// The public API7 consumer remains responsible for geometric and GPU validation.
bool encode_primitive_frame(std::shared_ptr<const PrimitiveFrame> source,
                            EncodedPrimitiveFrame& output,std::string& error) noexcept;

} // namespace a2fo::native11
