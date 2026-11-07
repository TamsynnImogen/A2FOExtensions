#pragma once
#include "presentation.hpp"
#include <d3d8.h>

namespace a2fo::refit_menu::loading_native {
// A transient managed texture: no default-pool resource or retained device
// survives Reset/destruction. Drawing never begins/ends/presents a scene.
bool draw_loading_image(IDirect3DDevice8* device, const Artwork& image);

// Opt in only for a known native loading owner. This holds a managed texture,
// which may retain its device: call reset() before native cleanup/device Reset
// or destruction, including native device teardown during loading and every
// kill-splash entry. Do not rely on static or
// DLL destruction to release it. The identity is weak and requires that owner
// lifetime contract; pointer equality cannot identify a destroyed device.
class LoadingTextureCache {
public:
    struct Stats { std::uint64_t creations{}, uploads{}; };
    LoadingTextureCache() = default;
    LoadingTextureCache(const LoadingTextureCache&) = delete;
    LoadingTextureCache& operator=(const LoadingTextureCache&) = delete;
    void reset();
    Stats stats() const { return stats_; }
private:
    friend bool draw_loading_image(IDirect3DDevice8*, const Artwork&, LoadingTextureCache&, std::uint64_t);
    friend bool draw_loading_layers(IDirect3DDevice8*, const Artwork&, const Artwork&, int, int,
                                    LoadingTextureCache*, LoadingTextureCache*, std::uint64_t, std::uint64_t);
    IDirect3DTexture8* texture_{};
    IDirect3DDevice8* device_{}; // weak identity, never AddRef'd directly
    UINT width_{}, height_{}, texture_width_{}, texture_height_{};
    std::uint64_t revision_{};
    bool uploaded_{};
    bool alpha_preserved_{};
    Stats stats_{};
};

// revision must change whenever image pixels change. The caller owns the
// loading-session cache and its explicit reset boundary.
bool draw_loading_image(IDirect3DDevice8* device, const Artwork& image,
                        LoadingTextureCache& cache, std::uint64_t revision);

// Draw an opaque scene at output size, then an output-sized ARGB UI layer.
// Cache revisions cover pixel changes; callers reset caches before native
// device cleanup/Reset. Null cache pointers use transient managed textures.
bool draw_loading_layers(IDirect3DDevice8* device, const Artwork& background,
                         const Artwork& overlay, int out_width, int out_height,
                         LoadingTextureCache* background_cache, LoadingTextureCache* overlay_cache,
                         std::uint64_t background_revision, std::uint64_t overlay_revision);
}
