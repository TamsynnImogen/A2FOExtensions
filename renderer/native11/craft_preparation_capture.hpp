#pragma once
#include "craft_preparation.hpp"
#include "world_model_association.hpp"
#include <memory>
namespace a2fo::native11 {
struct CraftPreparationReadProfile {
    std::uint32_t instance_vtable=0,database_vtable=0;
    std::uint32_t setup_function=0,hull_divisor_function=0,borg_cache_pointer=0;
    bool fleet_cache=false;
};
struct CapturedCraftPreparation {
    WorldMember member;
    std::shared_ptr<const ModelAsset> model;
    std::shared_ptr<const LightClassRecord> object_class;
    std::uint32_t database=0,entity_vtable=0,race=0,systems=0,borg_cache=0;
    std::uint32_t health_mode=0,native_x87_control=0;
    std::uint32_t fleet_cache_pointer=0,fleet_cached_texture=0;
    std::int32_t fleet_cached_index=-1;
    CraftPreparationInput input;
    PreparedCraftState state;
};
// Exact captured identity, logical inputs and derived state (including owners).
bool same_craft_preparation(const CapturedCraftPreparation&,const CapturedCraftPreparation&) noexcept;
// The caller qualifies the exact original setup body or its checked Fleet
// saved-original wrapper plus getter code. This reader calls neither. It reads
// logical entity/system/race fields, not the last rendered instance's 9c..a4.
// Both passes pin the same class version and validate the supplied DB/name.
// Caller brackets model version, stable world membership, source and frame.
// The Fleet cache is copied correlation data, not selected-texture authority.
// Failure leaves output unchanged; no class/part/FOW/material completeness claim.
bool capture_craft_preparation(const WorldModelAssociation&,CraftPreparationReadProfile,
    std::uint32_t native_x87_control,void* read_context,Read,
    void* class_context,PinLightClass,CapturedCraftPreparation&,std::string& error) noexcept;
}
