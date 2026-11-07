#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
constexpr std::uint32_t visibility_none=UINT32_MAX;
struct VisibilityNode {
    std::uint32_t parent=visibility_none,first_child=visibility_none;
    std::uint32_t next_sibling=visibility_none,flags=0;
};
struct CraftVisibilityInput {
    // Initial flags come from a stable owned template, never a previous
    // instance's mutable flags. All class/health inputs must be fresh.
    std::vector<VisibilityNode> nodes;
    std::uint32_t root=visibility_none;
    bool primary_database=true;
    std::uint32_t database_damage=visibility_none,damage_child=visibility_none;
    std::array<std::uint32_t,5> system_groups{
        visibility_none,visibility_none,visibility_none,visibility_none,visibility_none};
    std::array<bool,5> system_hidden{};
    std::uint32_t assimilation_group=visibility_none;
    bool assimilated=false;
    std::uint32_t base_group=visibility_none;
    bool instance_hidden=false,context_enabled=true;
    float hull_fraction=1;
};
struct CraftVisibilityResult {
    std::vector<std::uint32_t> flags;
    std::vector<bool> visible;
    float damage_threshold=0;
    // Low bits WRITTEN by class policy, including stores of the existing
    // value. LOD equal-selection paths make write intent significant.
    std::vector<std::uint8_t> low_bit_writes;
};
// CPU-only flag policy. Role aliases are applied in native order. Own bit 0
// hides a node; ancestor bit 1 prunes descendants. Does not certify game pose,
// class selection, materials, LOD, or complete geometry. Output is atomic.
bool evaluate_craft_visibility(const CraftVisibilityInput&,
    CraftVisibilityResult&,std::string& error) noexcept;
} // namespace a2fo::native11
