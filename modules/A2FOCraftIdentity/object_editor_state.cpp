#include "object_editor_state.hpp"
#include "../../sdk/include/a2fo_shield_values.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace a2fo::object_editor {
namespace {

bool space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && space(text.front())) text.remove_prefix(1);
    while (!text.empty() && space(text.back())) text.remove_suffix(1);
    return text;
}

int hex(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

bool valid_state(const SavedState& s) noexcept {
    const SavedState expected{};
    return s.magic == expected.magic && s.version == 1 &&
        s.size == sizeof(s) && (s.flags & ~(kCaptain | kRegistry | kShields)) == 0 &&
        std::memchr(s.captain.data(), 0, s.captain.size()) &&
        std::memchr(s.registry.data(), 0, s.registry.size()) &&
        (!(s.flags & kShields) ||
         a2fo::valid_shield_values(s.current.data(), s.maximum.data()));
}

bool parse_shield_number(const char* text, float* value) noexcept {
    if (!text || !value) return false;
    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(text, &end);
    if (end == text || errno == ERANGE || !std::isfinite(parsed)) return false;
    while (*end && space(*end)) ++end;
    if (*end) return false;
    *value = parsed;
    return true;
}

PrefixResult decode_prefix(std::string_view bytes, bool binary, bool tagged,
                           SavedState* state, std::size_t* consumed) noexcept {
    if (consumed) *consumed = 0;
    if (!state || !consumed) return PrefixResult::invalid;
    SavedState candidate{};
    std::size_t used = 0;
    if (binary) {
        const std::size_t header = tagged ? 8 : 0;
        if (bytes.size() < header + candidate.magic.size())
            return PrefixResult::absent;
        if (std::memcmp(bytes.data() + header, candidate.magic.data(),
                        candidate.magic.size()) != 0) return PrefixResult::absent;
        if (tagged) {
            std::uint32_t type = 0, count = 0;
            std::memcpy(&type, bytes.data(), 4);
            std::memcpy(&count, bytes.data() + 4, 4);
            if ((type & 0xffu) != 0 || count != sizeof(candidate))
                return PrefixResult::invalid;
        }
        if (bytes.size() < header + sizeof(candidate)) return PrefixResult::invalid;
        std::memcpy(&candidate, bytes.data() + header, sizeof(candidate));
        used = header + sizeof(candidate);
    } else {
        const auto newline = bytes.find('\n');
        const auto line = bytes.substr(0, newline);
        const auto equals = line.find('=');
        if (equals == std::string_view::npos ||
            trim(line.substr(0, equals)) != kSaveLabel) return PrefixResult::absent;
        const auto encoded = trim(line.substr(equals + 1));
        if (newline == std::string_view::npos ||
            encoded.size() != sizeof(candidate) * 2) return PrefixResult::invalid;
        auto* output = reinterpret_cast<unsigned char*>(&candidate);
        for (std::size_t i = 0; i < sizeof(candidate); ++i) {
            const int high = hex(encoded[i * 2]);
            const int low = hex(encoded[i * 2 + 1]);
            if (high < 0 || low < 0) return PrefixResult::invalid;
            output[i] = static_cast<unsigned char>((high << 4) | low);
        }
        used = newline + 1;
    }
    if (!valid_state(candidate)) return PrefixResult::invalid;
    *state = candidate;
    *consumed = used;
    return PrefixResult::present;
}

}  // namespace a2fo::object_editor
