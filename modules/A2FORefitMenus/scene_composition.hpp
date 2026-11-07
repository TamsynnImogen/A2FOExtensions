#pragma once

#include "presentation.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace a2fo::refit_menu::scene {

// Remembers native backgrounds before the native form paints its labels.
class Backgrounds {
public:
    void remember(std::string key, const Artwork& base);
    // Checks whether a saved background matches without creating output pixels.
    bool matches(const Artwork& current) const;
    // Identifies cloned native bitmap objects by their saved background pixels.
    std::string key(const Artwork& current) const;
    // Replaces only unchanged background pixels. On failure, out is untouched.
    bool compose(const Artwork& current, const Artwork& live, Artwork& out) const;

private:
    struct Saved {
        std::string key;
        Artwork base;
    };
    std::vector<Saved> saved_;
    std::size_t total_pixels_ = 0;
    const Saved* matching_base(const Artwork& current) const;
};

} // namespace a2fo::refit_menu::scene
