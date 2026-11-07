#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsA1RelationshipOp)(void*,std::uint32_t,
    std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint8_t a2fo_rs_stage8_a1_relationship_restore(void*,A2foRsA1RelationshipOp);
void a2fo_rs_stage8_a1_relationships_before_elimination(void*,A2foRsA1RelationshipOp);
}
