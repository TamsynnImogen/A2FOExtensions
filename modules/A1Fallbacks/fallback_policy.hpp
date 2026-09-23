#pragma once

#include <string>
#include <string_view>

namespace a1fallbacks {

constexpr std::size_t kMaximumSpriteNameLength = 255;

struct SpriteFallbackNames {
    std::string build_button;
    std::string faction_icon;
};

// Armada derives build-button and faction-icon sprite keys from these exact
// affixes. Empty or overlong inputs deliberately produce an empty candidate.
SpriteFallbackNames sprite_fallback_names(
    std::string_view object_basename,
    std::string_view faction_name);

}  // namespace a1fallbacks
