#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace a2fo::squadrons {

// Engineering bounds, not a claim about native selection/command capacity.
inline constexpr std::uint32_t kMaxMemberRows = 16;
inline constexpr std::uint32_t kMaxMembers = 32;
inline constexpr std::uint32_t kMaxSquadsPerTeam = 256;
inline constexpr std::uint32_t kMaxSquads = 4096;

using OdfFields = std::vector<std::pair<std::string, std::string>>;

struct MemberRow {
    std::uint32_t row = 0;
    std::string odf;
    std::uint32_t count = 0;
};

struct Definition {
    std::string odf;
    std::vector<MemberRow> members;
    bool reinforce_at_yard = true;
    std::uint32_t size() const noexcept;
};

enum class MemberClass { missing, mobile_craft, unsupported, squadron };
// The native adapter must establish actual class capability; guessing from an
// ODF name or assuming that every GameObjectClass is a mobile Craft is unsafe.
using ClassResolver = std::function<MemberClass(const std::string&)>;

struct DefinitionResult {
    std::optional<Definition> definition;
    std::string error;
    explicit operator bool() const noexcept { return definition.has_value(); }
};

// Accepts effective, inherited ODF fields (as supplied by the core SDK), not
// raw ODF text. Presentation metadata remains native-owned; build economics
// are derived separately from the completed native member classes.
DefinitionResult parse_definition(const std::string& odf, const OdfFields& fields);
std::string validate_definition(const Definition& definition,
                                const ClassResolver& resolve);
// Canonical, extension-less basename. Empty on invalid/path-bearing input.
std::string odf_key(std::string value);

struct BuildEconomics {
    // Native crew/officer/dilithium/latinum/metal/biomatter, then A2FO's four.
    std::array<std::int32_t, 10> costs{};
    float seconds = 0;
};
using EconomicsResolver = std::function<std::optional<BuildEconomics>(const std::string&)>;
// Transactional: output is unchanged if any member is absent/invalid or the
// count-weighted sum cannot be represented by the native fields.
std::string sum_build_economics(const Definition& definition,
    const EconomicsResolver& resolve, BuildEconomics* output);

} // namespace a2fo::squadrons
