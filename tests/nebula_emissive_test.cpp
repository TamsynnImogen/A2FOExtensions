#include "../core/nebula_emissive.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace a2fo::nebula;
    const std::array<std::uint32_t, kEmissiveSystemCount> pixels{
        0xffc01020u, 0xff20d010u, 0xff1020e0u,
        0xff808080u, 0xff4080c0u, 0xfff04020u};

    assert(combine_emissive_pixel(pixels, 0) == 0xff000000u);
    assert(combine_emissive_pixel(pixels, 1u << warp) == 0xffc01020u);
    assert(combine_emissive_pixel(
               pixels, (1u << warp) | (1u << impulse)) ==
           0xffc0d020u);
    assert(combine_emissive_pixel(pixels, 0x3fu) == 0xfff0d0e0u);

    assert(classify_subsystem_light(true, false, 100, 100.0, 0.0f) ==
           SubsystemLightState::operational);
    assert(classify_subsystem_light(false, true, 100, 100.0, 0.0f) ==
           SubsystemLightState::disabled);
    assert(classify_subsystem_light(false, false, 100, 100.0, 2.0f) ==
           SubsystemLightState::disabled);
    assert(classify_subsystem_light(false, false, 100, 100.0, 0.0f) ==
           SubsystemLightState::disabled);
    assert(classify_subsystem_light(false, false, 100, 0.0, 0.0f) ==
           SubsystemLightState::destroyed);
    assert(classify_subsystem_light(false, false, 100, 50.0, 0.0f) ==
           SubsystemLightState::destroyed);
    assert(classify_subsystem_light(false, true, 100, 0.0, 0.0f) ==
           SubsystemLightState::destroyed);
    assert(classify_subsystem_light(false, false, 0, 0.0, 0.0f) ==
           SubsystemLightState::operational);

    assert(normalize_texture_key("fbattle") == "fbattle");
    assert(normalize_texture_key("  Fbattle.tga  ") == "fbattle");
    assert(normalize_texture_key("Textures\\RGB\\Fbattle.DDS") ==
           "fbattle");
    assert(normalize_texture_key("textures/Compressed/hull.extra.png") ==
           "hull.extra");
    assert(normalize_texture_key("   ").empty());

    std::vector<std::uint32_t> bloom_pixels(9u * 9u, 0xff000000u);
    bloom_pixels[4u * 9u + 4u] = 0xffc00000u;
    assert(add_emissive_bloom(bloom_pixels, 9, 9, 1, 100));
    assert(((bloom_pixels[4u * 9u + 4u] >> 16) & 0xffu) >= 0xc0u);
    assert(((bloom_pixels[4u * 9u + 3u] >> 16) & 0xffu) != 0u);
    assert((bloom_pixels[4u * 9u + 3u] & 0xffffu) == 0u);
    assert(bloom_pixels[0] == 0xff000000u);
    assert(!add_emissive_bloom(bloom_pixels, 8, 9, 1, 100));
    return 0;
}
