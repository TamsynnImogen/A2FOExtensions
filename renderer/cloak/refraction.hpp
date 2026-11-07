#pragma once

// D3D8/D3D9-neutral entry point. The caller retains ownership of device8.
namespace a2fo::cloak {

struct Parameters {
    float clip[16]{};                 // Native VS 1.1 c2..c5 rows.
    float lights[19][4]{};           // Native compositor c8..c26.
    float visibility = 0.0f;
    float time = 0.0f;
    float hull_floor = 0.1f;
    float bend = 0.035f;
    float ripple = 0.08f;
    float rim = 0.35f;
    float front = 1.0f;
    float shimmer = 0.02f;
    unsigned long long snapshot_key = 0; // Same nonzero key reuses one pre-hull scene copy.
    float bounds_center[3]{};
    float bounds_extent[3]{1.0f, 1.0f, 1.0f};
};

struct Draw {
    unsigned primitive_type = 4; // D3DPT_TRIANGLELIST
    int base_vertex = 0;
    unsigned min_vertex = 0;
    unsigned num_vertices = 0;
    unsigned start_index = 0;
    unsigned primitive_count = 0;
};

enum class Outcome { fallback, drawn, fatal };

class Refraction {
public:
    Refraction();
    ~Refraction();
    Refraction(const Refraction&) = delete;
    Refraction& operator=(const Refraction&) = delete;
    // Fatal means D3D9 DIP was attempted or restoration failed: consume the call
    // and disable this pass; native replay could duplicate partially emitted work.
    Outcome draw(void* device8, const Parameters&, const Draw&) noexcept;
    // Call before reset or device replacement; default-pool resources cannot survive reset.
    void release() noexcept;
    const char* diagnostic() const noexcept;
private:
    struct Impl;
    Impl* impl_;
};

} // namespace a2fo::cloak
