#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace a2fo::native11 {
// A checked native class-table row. Tokens are opaque: this policy never
// dereferences them or establishes class/model/texture ownership itself.
struct CraftBorgMaterialRow {
    std::uint32_t node_index=0,original_texture=0,borg_texture=0,slot_count=0;
};
struct CraftMaterialSlotWrite {
    std::uint32_t node_index=0,slot=0,texture=0;
};
// Pinned Root base/Borg operation on existing 4..8-slot mesh vectors only.
// Base writes auxiliary slot 3 before diffuse slot 0; Borg writes slot 0 only.
// Unwritten slots must be supplied by an independently owned prior state.
// Empty input yields an explicit empty write list, never absence authority.
// No native calls or state reads. Rejects duplicate/out-of-range node indices,
// zero alternates, invalid vector bounds and >4096 rows; failure preserves out.
bool craft_borg_material_writes(const std::vector<CraftBorgMaterialRow>&,
    std::uint8_t borg_flag,std::vector<CraftMaterialSlotWrite>& out,
    std::string& error) noexcept;
} // namespace a2fo::native11
