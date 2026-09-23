#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace a2fo::faction_suffix {

inline constexpr char kCommand[] = "factionTextureSuffix";
inline constexpr std::size_t kMaximumLength = 32;

// Shared filename-suffix policy used by both A2FOTextureVariants and
// A2FOODFVariants. Empty is valid and disables a faction-specific suffix.
inline bool normalize(std::string_view input,
                      std::string* normalized) noexcept {
    if (!normalized) return false;
    try {
        std::size_t first = 0;
        while (first < input.size() &&
               std::isspace(static_cast<unsigned char>(input[first]))) {
            ++first;
        }
        std::size_t last = input.size();
        while (last > first &&
               std::isspace(static_cast<unsigned char>(input[last - 1]))) {
            --last;
        }
        std::string candidate(input.substr(first, last - first));
        if (candidate.size() > kMaximumLength) return false;
        for (char character : candidate) {
            const unsigned char value =
                static_cast<unsigned char>(character);
            if (!std::isalnum(value) && character != '_' &&
                character != '-') {
                return false;
            }
        }
        *normalized = std::move(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace a2fo::faction_suffix
