#pragma once
#include "scene.hpp"
#include "texture.hpp"
#include "part_state.hpp"
#include "model_geometry_policy.hpp"
#include "live_material.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
enum class AssetKind { model, texture };
using ReadAsset=bool(*)(void* context,AssetKind kind,const std::string& identifier,
                       std::vector<std::uint8_t>& bytes,std::string& error);
using ResolveTexture=bool(*)(void* context,const std::string& model_identifier,
                             std::uint32_t part_index,const std::string& authored_texture,
                             std::string& resolved_texture,std::string& error);
struct AssetReader {
    void* context=nullptr;
    ReadAsset read=nullptr;
    ResolveTexture resolve_texture=nullptr;
    ResolvePartState resolve_part_state=nullptr;
    ResolveModelGeometryPolicy resolve_geometry_policy=nullptr;
    ResolveLiveMaterial resolve_live_material=nullptr;
};
struct ContentTexture {
    std::string identifier;
    TextureFormat format=TextureFormat::bgra8;
    TextureAlpha alpha=TextureAlpha::unknown;
    std::uint32_t width=0,height=0;
    std::vector<TextureMip> mips;
    std::vector<std::uint8_t> bytes;
};
inline constexpr std::size_t no_texture=static_cast<std::size_t>(-1);
struct ContentPart {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    float local_to_model[16]{};
    float colour[4]{};
    float emissive[3]{};
    std::uint32_t flags=0;
    std::size_t texture_index=no_texture;
    MaterialSeed seed;
    LiveMaterial live_material;
};
struct ModelContent {
    std::vector<ContentPart> parts;
    std::vector<ContentTexture> textures;
};
bool load_model_content(AssetReader reader,const std::string& model_identifier,
                        ModelContent& output,std::string& error,
                        bool all_model_parts=false) noexcept;
bool load_content_texture(AssetReader reader,const std::string& identifier,
                          ContentTexture& output,std::string& error) noexcept;
} // namespace a2fo::native11
