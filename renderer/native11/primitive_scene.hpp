#pragma once

#include "renderer.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {

struct PrimitiveMesh {
    std::uint64_t id=0;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
struct PrimitiveTexture {
    std::uint64_t id=0;
    std::uint32_t width=0,height=0;
    std::vector<std::uint8_t> bgra; // tightly packed, top row first
};
struct PrimitiveDraw {
    std::uint64_t mesh=0,texture=0; // texture zero selects implicit white
    Draw surface{}; // mesh and texture handles must remain zero
};
struct PrimitiveFrame {
    std::uint64_t generation=0,source_epoch=0;
    FrameView settings{}; // draw/light pointers and counts must remain zero
    std::vector<Light> lights;
    std::vector<PrimitiveMesh> meshes; // full resource manifest on every packet
    std::vector<PrimitiveTexture> textures;
    std::vector<PrimitiveDraw> draws;
};

// Externally serialized, owner-thread frontend. The Renderer must outlive this
// object; clear or destroy on that same thread before destroying the Renderer.
class PrimitiveScene {
public:
    explicit PrimitiveScene(Renderer&);
    ~PrimitiveScene();
    PrimitiveScene(const PrimitiveScene&)=delete;
    PrimitiveScene& operator=(const PrimitiveScene&)=delete;
    PrimitiveScene(PrimitiveScene&&)=delete;
    PrimitiveScene& operator=(PrimitiveScene&&)=delete;
    HRESULT render(const PrimitiveFrame&,std::string& error) noexcept;
    void clear() noexcept; // retains successful generation/source watermarks
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace a2fo::native11
