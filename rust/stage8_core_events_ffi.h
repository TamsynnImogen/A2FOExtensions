#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreEventStep)(void*,std::uint32_t,std::uintptr_t,std::uintptr_t);
struct A2foRsProducerEventRules {
    std::uint32_t size,maximum,destroying,admit,finishing,starting_effect;
};
struct A2foRsReplacementRules {std::uint32_t size,known_flags,inherit_position,inherit_rotation,maximum_owner,original_owner;};
std::uint64_t a2fo_rs_stage8_core_resolve_destroyed(const A2foRsReplacementRules*,A2foRsCoreEventStep,void*);
std::uint64_t a2fo_rs_stage8_core_spawn_replacement(A2foRsCoreEventStep,void*);
std::uint32_t a2fo_rs_stage8_core_object_hash(const std::uint8_t*,std::size_t,std::uint32_t,std::int32_t,const std::uint8_t*,std::size_t);
std::uint64_t a2fo_rs_stage8_core_register_handler(A2foRsCoreEventStep,void*);
std::uint64_t a2fo_rs_stage8_core_dispatch_producer(const A2foRsProducerEventRules*,A2foRsCoreEventStep,void*);
}
