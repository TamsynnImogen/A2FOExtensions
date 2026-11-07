#include "scene_composition.hpp"
#include "../../rust/ui_ffi.h"
#include <utility>
namespace a2fo::refit_menu::scene {
namespace {
constexpr std::size_t max_pixels = 8'000'000;
constexpr std::size_t max_backgrounds = 16;
A2foRsArtwork pack(const Artwork &a) {
    return {a.width, a.height, a.pixels.data(), a.pixels.size()};
}
} // namespace
void Backgrounds::remember(std::string key, const Artwork &base) {
    const auto image = pack(base);
    if (!a2fo_rs_ui_valid_art(&image))
        return;
    for (auto it = saved_.begin(); it != saved_.end(); ++it)
        if (it->key == key) {
            total_pixels_ -= it->base.pixels.size();
            saved_.erase(it);
            break;
        }
    const auto count = base.pixels.size();
    while (!saved_.empty() &&
           (saved_.size() >= max_backgrounds || total_pixels_ > max_pixels - count)) {
        total_pixels_ -= saved_.front().base.pixels.size();
        saved_.erase(saved_.begin());
    }
    saved_.push_back({std::move(key), base});
    total_pixels_ += count;
}
const Backgrounds::Saved *Backgrounds::matching_base(const Artwork &current) const {
    const auto image = pack(current);
    std::vector<A2foRsSavedArtwork> candidates;
    candidates.reserve(saved_.size());
    for (const auto &s : saved_)
        candidates.push_back(
            {reinterpret_cast<const std::uint8_t *>(s.key.data()), s.key.size(), pack(s.base)});
    const auto index = a2fo_rs_ui_match(&image, candidates.data(), candidates.size());
    return index < 0 ? nullptr : &saved_[static_cast<std::size_t>(index)];
}
bool Backgrounds::matches(const Artwork &current) const {
    return matching_base(current) != nullptr;
}
std::string Backgrounds::key(const Artwork &current) const {
    const auto *s = matching_base(current);
    return s ? s->key : std::string{};
}
bool Backgrounds::compose(const Artwork &current, const Artwork &live, Artwork &out) const {
    const auto l = pack(live);
    if (!a2fo_rs_ui_valid_art(&l))
        return false;
    const auto *chosen = matching_base(current);
    if (!chosen)
        return false;
    Artwork separate;
    Artwork &result = &out == &current || &out == &live ? separate : out;
    result.pixels.resize(current.pixels.size());
    const auto c = pack(current), b = pack(chosen->base);
    if (!a2fo_rs_ui_compose(&c, &b, &l, reinterpret_cast<const std::uint8_t *>(chosen->key.data()),
                            chosen->key.size(), result.pixels.data(), result.pixels.size()))
        return false;
    result.width = current.width;
    result.height = current.height;
    if (&result != &out)
        out = std::move(result);
    return true;
}
} // namespace a2fo::refit_menu::scene
