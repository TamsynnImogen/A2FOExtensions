#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
struct A2foStage8SolCursor {
    std::uintptr_t base;
    std::uintptr_t cursor;
    std::uintptr_t end;
    std::uint32_t buffer_size;
    std::uintptr_t offset;
    std::uintptr_t remaining;
};
bool a2fo_rs_c_sol_cursor(A2foStage8SolCursor*);
bool a2fo_rs_c_sol_target(const A2foStage8SolCursor*,std::size_t,std::uintptr_t*);
bool a2fo_rs_c_sol_object_span(bool,std::size_t,std::uint32_t);
bool a2fo_rs_c_sol_object_record(const std::uint8_t*,std::uint32_t);
bool a2fo_rs_c_sol_mission_span(std::size_t,std::uint32_t);
bool a2fo_rs_c_sol_mission_record(const std::uint8_t*);
std::uint32_t a2fo_rs_c_sol_join_action(const std::uint8_t*,std::size_t,
    const std::uint8_t*,std::size_t);
std::size_t a2fo_rs_c_sol_basename_start(const std::uint8_t*,std::size_t);
using A2foStage8SolLower = std::uint8_t (*)(std::uint8_t);
void a2fo_rs_c_sol_lower_ascii(std::uint8_t*,std::size_t,A2foStage8SolLower);
bool a2fo_rs_c_sol_has_extension(const std::uint8_t*,std::size_t);
using A2foStage8SolResolveStep = std::int32_t (*)(void*,std::uint32_t);
bool a2fo_rs_c_sol_resolve_source(void*,const std::uint8_t*,std::size_t,
    A2foStage8SolResolveStep);
using A2foStage8SolRoot = std::int32_t (*)(void*,std::uint32_t);
bool a2fo_rs_c_sol_index_roots(void*,std::uint32_t,A2foStage8SolRoot);
using A2foStage8SolDirectoryStep = std::int32_t (*)(void*,std::uint32_t,bool*);
bool a2fo_rs_c_sol_walk_directory(void*,A2foStage8SolDirectoryStep);
using A2foStage8SolMapDetailsStep = std::uintptr_t (*)(void*,std::uint32_t,
    std::size_t);
std::uintptr_t a2fo_rs_c_sol_create_map_details(void*,A2foStage8SolMapDetailsStep);
std::uintptr_t a2fo_rs_c_sol_load_main(void*,A2foStage8SolMapDetailsStep);
bool a2fo_rs_c_sol_spawn(void*,A2foStage8SolMapDetailsStep);
std::uintptr_t a2fo_rs_c_sol_create_selected(void*,bool,A2foStage8SolMapDetailsStep);
std::uintptr_t a2fo_rs_c_sol_load_stage(void*,std::uint32_t,A2foStage8SolMapDetailsStep);
std::uintptr_t a2fo_rs_c_sol_probe(void*,A2foStage8SolMapDetailsStep);
using A2foStage8SolInitStep = std::int32_t (*)(void*,std::uint32_t,std::size_t);
bool a2fo_rs_c_sol_module_init(void*,A2foStage8SolInitStep);
void a2fo_rs_c_sol_discovery(void*,A2foStage8SolInitStep);
std::uintptr_t a2fo_rs_c_sol_map_hook(void*,bool,A2foStage8SolMapDetailsStep);
std::uintptr_t a2fo_rs_c_sol_ai_guard(void*,A2foStage8SolMapDetailsStep);
bool a2fo_rs_c_sol_nebula_guard(void*,A2foStage8SolMapDetailsStep);
bool a2fo_rs_c_sol_summary(void*,bool,bool,std::uint32_t,bool,bool,
    A2foStage8SolInitStep);
bool a2fo_rs_c_sol_lifecycle(void*,std::uint32_t,A2foStage8SolInitStep);
}
