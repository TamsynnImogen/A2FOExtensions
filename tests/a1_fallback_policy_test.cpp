#include "fallback_policy.hpp"

#include <cassert>
#include <string>

int main() {
    const auto federation = a1fallbacks::sprite_fallback_names(
        "fbase", "federation");
    assert(federation.build_button == "b_fbase");
    assert(federation.faction_icon == "federation_icon");

    const auto custom = a1fallbacks::sprite_fallback_names(
        "my_station", "cardassian");
    assert(custom.build_button == "b_my_station");
    assert(custom.faction_icon == "cardassian_icon");

    const auto missing = a1fallbacks::sprite_fallback_names("", "");
    assert(missing.build_button.empty());
    assert(missing.faction_icon.empty());

    const std::string maximum(
        a1fallbacks::kMaximumSpriteNameLength - 2, 'x');
    assert(a1fallbacks::sprite_fallback_names(maximum, "fed")
               .build_button.size() ==
           a1fallbacks::kMaximumSpriteNameLength);
    assert(a1fallbacks::sprite_fallback_names(maximum + "x", "fed")
               .build_button.empty());
}
