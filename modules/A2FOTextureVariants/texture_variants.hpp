#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace a2fo::texture_variants {

constexpr std::size_t kMaximumFactionSuffixLength = 32;
constexpr std::size_t kMaximumFactionNodeNameLength = 63;

enum class SubsystemCondition : std::uint8_t {
    operational,
    disabled,
    destroyed,
};

// Empty input is a valid way to disable a faction suffix. Non-empty suffixes
// are deliberately limited to filename-safe ASCII so an ODF cannot turn a
// texture suffix into a path traversal.
bool normalize_faction_suffix(std::string_view input,
                              std::string* normalized) noexcept;

// Normalizes the internal Race ODF `name` used for faction SOD nodes. Names
// are case-insensitive and deliberately limited to the identifier characters
// used by Armada race definitions and Storm3D node names.
bool normalize_faction_node_name(std::string_view input,
                                 std::string* normalized) noexcept;

// Armada's native Borg node uses bits 0 and 1 together: both are cleared for
// the owning race and set for every other race. Preserve all unrelated flags.
std::uint32_t faction_node_flags(std::uint32_t current,
                                 bool selected) noexcept;

// Returns the extensionless texture name passed to ST3D_Texture::Find.
// Existing .tga/.dds extensions are removed and an already-present suffix is
// not appended twice.
std::string faction_texture_name(std::string_view diffuse_name,
                                 std::string_view suffix);

// Returns the relative path used by Fleet Operations' own texture preflight,
// for example Textures\fed_hull_k.dds.
std::string texture_asset_path(std::string_view texture_name,
                               std::string_view extension);

// Mirrors CraftSystem's distinction between a temporarily disabled system
// and one which is genuinely destroyed/damaged. Invalid native values fail
// open to the operational byte, matching the renderer's existing policy.
SubsystemCondition subsystem_condition(bool operational,
                                       bool forced_disabled,
                                       std::int32_t maximum_hitpoints,
                                       double current_hitpoints,
                                       float disable_time) noexcept;

// A deterministic pseudo-random choice keeps every multiplayer peer on the
// same damaged mesh without depending on the process-global C RNG.
std::size_t subsystem_mesh_choice(std::uint32_t craft_handle,
                                  std::size_t subsystem_index,
                                  std::uint32_t destruction_count,
                                  std::size_t choice_count) noexcept;

// Converts live subsystem hitpoints into the visual scale used while a
// destroyed part is being reconstructed. Zero remains fully absent; positive
// repair progress eases from a small seed to the complete mesh.
float subsystem_rebuild_scale(std::int32_t maximum_hitpoints,
                              double current_hitpoints) noexcept;

// Produces a deterministic unit-interval sample for moving repair effects.
// Keeping this independent of the process-global RNG makes cosmetic placement
// repeatable on multiplayer peers.
float subsystem_repair_sample(std::uint32_t craft_handle,
                              std::size_t subsystem_index,
                              std::uint32_t sequence,
                              std::size_t component) noexcept;

}  // namespace a2fo::texture_variants
