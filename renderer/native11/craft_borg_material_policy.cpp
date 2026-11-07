#include "craft_borg_material_policy.hpp"
#include <array>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* reason) noexcept {
    try {error=reason;}catch(...){}return false;
}
}
bool craft_borg_material_writes(const std::vector<CraftBorgMaterialRow>& rows,
    std::uint8_t borg_flag,std::vector<CraftMaterialSlotWrite>& out,
    std::string& error) noexcept {
    try {
        if(rows.size()>4096)return fail(error,"too many Craft Borg material rows");
        std::array<bool,4096> seen{};
        std::vector<CraftMaterialSlotWrite> pending;
        pending.reserve(rows.size()*(borg_flag?1u:2u));
        for(const auto& row:rows){
            if(row.node_index>=seen.size()||seen[row.node_index]||
               row.slot_count<4||row.slot_count>8||!row.borg_texture)
                return fail(error,"invalid or duplicate Craft Borg material row");
            seen[row.node_index]=true;
            if(borg_flag)pending.push_back({row.node_index,0,row.borg_texture});
            else {
                pending.push_back({row.node_index,3,row.original_texture});
                pending.push_back({row.node_index,0,row.original_texture});
            }
        }
        out=std::move(pending);error.clear();return true;
    }catch(...){return fail(error,"Craft Borg material write allocation failure");}
}
} // namespace a2fo::native11
