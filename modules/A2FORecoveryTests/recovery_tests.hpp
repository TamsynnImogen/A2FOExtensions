#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace a2fo::recovery_tests {

enum class Routine : std::uint32_t { engine_context = 0, node_transform = 1 };

struct Snapshot {
    Routine routine{};
    std::uint32_t object{};
    std::uint16_t control_word{};
    std::uint8_t fp_tag{};
    std::uint8_t reserved{};
    std::array<std::uint8_t, 0x88> node_before{};
    std::array<std::uint8_t, 0x88> node_after{};
    std::array<std::uint32_t, 3> context_addresses{};
    std::array<std::uint32_t, 3> context_before{};
    std::array<std::uint32_t, 3> context_after{};
    std::uint32_t actual_return{};
};

enum class Outcome { match, mismatch, skip };

struct Comparison {
    Outcome outcome{};
    const char* reason{};
    std::uint32_t expected_return{};
    std::uint32_t mismatch_offset{};
    std::uint32_t expected_word{};
    std::uint32_t actual_word{};
};

Comparison compare(const Snapshot&);
std::string record_json(const Snapshot&, const Comparison&);

} // namespace a2fo::recovery_tests
