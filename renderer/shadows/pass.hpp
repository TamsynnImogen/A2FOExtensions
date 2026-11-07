#pragma once

namespace a2fo::shadows {

struct Settings {
    bool enabled = false;
    unsigned size = 1024;
    unsigned max_draws = 2048;
    float strength = 0.55f;
    float bias = 0.0015f;
    // Negative keeps the legacy normalized-depth bias. Otherwise use a fixed
    // world-space separation, independent of unrelated captured geometry.
    float bias_world = -1.0f;
};

struct Draw {
    int base_vertex = 0;
    unsigned min_vertex = 0;
    unsigned num_vertices = 0;
    unsigned start_index = 0;
    unsigned primitive_count = 0;
};

struct Capture {
    float clip[16]{};
    float bounds_center[3]{};
    float bounds_extent[3]{};
    float light_direction[3]{};
};

class Pass {
public:
    Pass();
    ~Pass();
    Pass(const Pass&) = delete;
    Pass& operator=(const Pass&) = delete;
    void configure(Settings) noexcept;
    void begin_view() noexcept;
    bool capture(void* device8, const Capture&, const Draw&,
                 const float* triangle_positions, unsigned vertex_count) noexcept;
    bool finish_view() noexcept;
    void release() noexcept;
    const char* diagnostic() const noexcept;
    const char* capture_diagnostic() const noexcept;
#ifdef A2FO_SHADOW_TEST_PROBE
    bool probe(unsigned record,const float point[3],float out[4]) noexcept;
#endif
private:
    struct Impl;
    Impl* impl_;
};

} // namespace a2fo::shadows
