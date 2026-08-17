#pragma once

#include <cstdint>

namespace a2fo::instant_action_settings {

struct Bounds {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

struct EffectiveBounds {
    Bounds bounds{};
    bool valid = false;
    bool repaired = false;
};

EffectiveBounds effective_load_bounds(const Bounds& load,
                                      const Bounds& save) noexcept;

bool contains(const Bounds& bounds, std::int32_t x,
              std::int32_t y) noexcept;

bool is_load_settings_caption(const char* caption) noexcept;

}  // namespace a2fo::instant_action_settings
