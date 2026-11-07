#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
bool a2fo_rs_stage9_sol_log_pass16(std::int32_t pass);
const char* a2fo_rs_stage9_sol_directory(std::uint32_t index);
const char* a2fo_rs_stage9_sol_search_mask();
const char* a2fo_rs_stage9_sol_virtual_prefix();
const char* a2fo_rs_stage9_sol_virtual_mask();
const char* a2fo_rs_stage9_sol_selected_suffix(bool selected);
const char* a2fo_rs_stage9_sol_post_mission_message(bool second);
bool a2fo_rs_stage9_sol_mission_present(std::size_t length);
bool a2fo_rs_stage9_sol_probe_candidate(std::uint32_t version, std::uint32_t selected);
bool a2fo_rs_stage9_sol_probe_gate(std::uint32_t operation, bool reader,
                                   bool context, bool details, bool mission,
                                   bool list);
std::uint32_t a2fo_rs_stage9_sol_caller_version(std::uint32_t version,
                                                bool reader, bool context,
                                                bool details, bool mission,
                                                bool list);
std::uint32_t a2fo_rs_stage9_sol_compatible_version();
bool a2fo_rs_stage9_sol_counter_changed(std::int32_t now, std::int32_t before);
bool a2fo_rs_stage9_sol_spawn_complete(std::int32_t state);
bool a2fo_rs_stage9_sol_file_size(std::int64_t size);
bool a2fo_rs_stage9_sol_version_fits(std::uint64_t version);
const char* a2fo_rs_stage9_sol_empty_metadata();
std::uint32_t a2fo_rs_stage9_sol_direct_source_root();
bool a2fo_rs_stage9_sol_input_present(const std::uint8_t* value);
}
