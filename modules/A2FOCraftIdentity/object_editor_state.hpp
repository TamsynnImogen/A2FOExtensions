#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace a2fo::object_editor {

constexpr std::size_t kTextCapacity = 512;
constexpr char kSaveLabel[] = "a2fo_objectEditor";
constexpr std::uint32_t kCaptain = 1;
constexpr std::uint32_t kRegistry = 2;
constexpr std::uint32_t kShields = 4;

// Fixed, little-endian version-1 payload. No object pointers or STL objects
// enter the native stream. Its position identifies the owning Craft.
struct SavedState {
    std::array<char, 8> magic{{'A', '2', 'F', 'O', 'E', 'D', 'I', 'T'}};
    std::uint32_t version = 1;
    std::uint32_t size = 1076;
    std::uint32_t flags = 0;
    std::array<float, 4> current{};
    std::array<float, 4> maximum{};
    std::array<char, kTextCapacity> captain{};
    std::array<char, kTextCapacity> registry{};
};
static_assert(sizeof(SavedState) == 1076, "version-1 editor save ABI changed");

bool valid_state(const SavedState& state) noexcept;
bool parse_shield_number(const char* text, float* value) noexcept;

enum class PrefixResult { absent, present, invalid };

// Non-consuming probe of native reader bytes. Old records leave consumed=0.
// Native binary streams optionally tag each Out operation with {type,count};
// native text OutBytes writes "label = <hex>\r\n".
PrefixResult decode_prefix(std::string_view bytes, bool binary, bool tagged,
                           SavedState* state, std::size_t* consumed) noexcept;

}  // namespace a2fo::object_editor
