#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
bool a2fo_rs_stage9_sol_discovery_log_pass(std::int32_t pass);
std::size_t a2fo_rs_stage9_sol_parent_end(const std::uint8_t* bytes,
                                           std::size_t len);
bool a2fo_rs_stage9_sol_is_bzn_leaf(const std::uint8_t* bytes,
                                     std::size_t len);
}
