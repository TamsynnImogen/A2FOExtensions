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

FpqPathResult parse_fpq_odf_directories(const std::vector<std::uint8_t>& bytes);

}  // namespace a2fo
