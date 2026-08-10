/*
 * File: core/fpq_paths.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: FPQ archive metadata and traversal helpers used to discover mod assets inside packed extension containers.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace a2fo {

struct FpqPathResult {
    bool ok = false;
    std::uint32_t version = 0;
    std::uint32_t file_count = 0;
    std::vector<std::string> odf_directories;
    std::string error;
};

// Parses an in-memory FPQ metadata prefix and returns normalized ODF directory
// names. No Fleet Ops types or Windows APIs are required, so this is
// host-testable.
FpqPathResult parse_fpq_odf_directories(const std::vector<std::uint8_t>& bytes);

}  // namespace a2fo
