#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
using A2foStage9SolStep = std::int32_t (*)(void*, std::uint32_t);
using A2foStage9SolReadSlot = std::int32_t (*)(void*);
bool a2fo_rs_stage9_sol_root_directories(void*, A2foStage9SolStep);
bool a2fo_rs_stage9_sol_index_entry(void*, A2foStage9SolStep);
bool a2fo_rs_stage9_sol_file_exists(std::uint32_t attributes,
                                     std::uint32_t invalid_attributes,
                                     std::uint32_t directory_flag);
bool a2fo_rs_stage9_sol_title_filename(std::size_t title_size);
bool a2fo_rs_stage9_sol_type_empty(std::size_t type_size);
std::uint32_t a2fo_rs_stage9_sol_start_count(void* slot_context, bool slot_present,
                                              A2foStage9SolReadSlot read_slot,
                                              std::size_t starts);
std::uint32_t a2fo_rs_stage9_sol_nonnegative_counter(std::int32_t value);
std::uint32_t a2fo_rs_stage9_sol_count_size(std::size_t value);
std::uint32_t a2fo_rs_stage9_sol_increment_counter(std::uint32_t value);
std::uint32_t a2fo_rs_stage9_sol_sum_failures(std::uint32_t missing,
                                               std::uint32_t construct);
bool a2fo_rs_stage9_sol_log_first16(std::uint32_t count);
bool a2fo_rs_stage9_sol_log_index32(std::size_t index);
bool a2fo_rs_stage9_sol_label_odf(std::size_t instance_size);
void a2fo_rs_stage9_sol_record_mission_add(bool, std::uint32_t*);
}
