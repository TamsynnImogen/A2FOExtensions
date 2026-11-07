#pragma once
#include "primitive_scene.hpp"
#include "quad_geometry.hpp"
#include "texture_store.hpp"
#include <map>

namespace a2fo::native11 {
struct CapturedScreenSprite {
    ScreenSprite geometry{};
    float colour[4]{1,1,1,1};
    std::shared_ptr<const TextureAsset> texture;
    bool additive=false;
    Sampling sampling=Sampling::linear_clamp;
};
struct CapturedSpriteBatch {
    std::uint32_t view=0,width=0,height=0;
    std::uint64_t generation=0,source_ticket=0;
    std::vector<CapturedScreenSprite> sprites;
};
// Externally serialized CPU frontend. Owners supplied to update must remain
// immutable; published packets own their decoded pixels independently.
class SpriteFrameStream final {
public:
    bool update(const CapturedSpriteBatch&,std::uint64_t fresh_epoch,
                std::shared_ptr<const PrimitiveFrame>& out,std::string& error) noexcept;
    void clear() noexcept; // retains generation and fresh epoch watermarks
private:
    struct Cached {
        std::shared_ptr<const TextureAsset> owner;
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        PrimitiveTexture decoded;
    };
    std::map<std::uint64_t,Cached> cache_;
    std::shared_ptr<const PrimitiveFrame> current_;
    std::uint64_t last_generation_=0,last_fresh_epoch_=0,last_source_epoch_=0,last_ticket_=0;
    bool active_=false;
};
} // namespace a2fo::native11
