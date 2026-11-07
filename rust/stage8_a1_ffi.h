#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint8_t(*A2foRsA1Classify)(std::uint8_t,void*);
typedef std::uint8_t(*A2foRsA1Emit)(void*,std::uint8_t,std::uint32_t);
typedef std::uint8_t(*A2foRsA1Readable)(const std::uint8_t*,std::size_t,void*);
typedef std::int32_t(*A2foRsA1Compare)(const std::uint8_t*,const std::uint8_t*,void*);
typedef std::int32_t(*A2foRsA1NCompare)(const std::uint8_t*,const std::uint8_t*,std::size_t,void*);
typedef std::size_t(*A2foRsA1Length)(const std::uint8_t*,void*);
typedef const std::uint8_t*(*A2foRsA1Scan)(const std::uint8_t*,void*);
typedef std::uint64_t(*A2foRsA1Op)(void*,std::uint32_t);
std::uint8_t a2fo_rs_stage8_a1_neutral_odf(const std::uint8_t*,const std::uint8_t*,A2foRsA1Compare,void*);
std::uint8_t a2fo_rs_stage8_a1_scout_path(const std::uint8_t*,A2foRsA1Scan,A2foRsA1Compare,void*);
std::uint8_t a2fo_rs_stage8_a1_ascii_equal(const std::uint8_t*,const std::uint8_t*,A2foRsA1Classify,void*);
std::uint8_t a2fo_rs_stage8_a1_contains_ci(const std::uint8_t*,const std::uint8_t*,A2foRsA1Length,A2foRsA1Classify,void*);
std::uint8_t a2fo_rs_stage8_a1_numbered_family(const std::uint8_t*,const std::uint8_t*,A2foRsA1Compare,A2foRsA1NCompare,A2foRsA1Length,void*);
void a2fo_rs_stage8_a1_join_path(const std::uint8_t*,void*,A2foRsA1Op);
std::uint8_t a2fo_rs_stage8_a1_mod_chain(void*,A2foRsA1Op);
void a2fo_rs_stage8_a1_printable(const std::uint8_t*,std::uint8_t*,std::size_t,A2foRsA1Readable,void*);
void a2fo_rs_stage8_a1_race(const std::uint8_t*,std::uint8_t*,std::size_t);
std::uint8_t a2fo_rs_stage8_a1_comments(const std::uint8_t*,std::size_t,A2foRsA1Emit,void*);
std::uint8_t a2fo_rs_stage8_a1_identifier(std::uint8_t);
void a2fo_rs_stage8_a1_assignment(const std::uint8_t*,std::size_t,std::size_t*,std::size_t*);
void a2fo_rs_stage8_a1_token(const std::uint8_t*,std::uint8_t*,std::size_t,A2foRsA1Classify,void*);
std::uint32_t a2fo_rs_stage8_a1_bool(std::uint8_t*,std::size_t,A2foRsA1Classify,A2foRsA1Classify,void*);
}
