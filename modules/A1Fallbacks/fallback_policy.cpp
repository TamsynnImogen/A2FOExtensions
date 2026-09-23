#include "fallback_policy.hpp"

namespace a1fallbacks {
namespace {

std::string affixed_name(
    std::string_view prefix, std::string_view stem,
    std::string_view suffix) {
    if (stem.empty() ||
        prefix.size() + stem.size() + suffix.size() >
            kMaximumSpriteNameLength) {
        return {};
    }
    std::string result;
    result.reserve(prefix.size() + stem.size() + suffix.size());
    result.append(prefix.data(), prefix.size());
    result.append(stem.data(), stem.size());
    result.append(suffix.data(), suffix.size());
    return result;
}

}  // namespace

SpriteFallbackNames sprite_fallback_names(
    std::string_view object_basename,
    std::string_view faction_name) {
    return SpriteFallbackNames{
        affixed_name("b_", object_basename, {}),
        affixed_name({}, faction_name, "_icon")};
}

}  // namespace a1fallbacks
