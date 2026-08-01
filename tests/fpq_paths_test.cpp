#include "fpq_paths.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}
}

int main() {
    std::vector<std::uint8_t> minimal(0x1c + 12, 0);
    minimal[0] = 'F';
    minimal[1] = 'P';
    minimal[2] = 'Q';
    put_u32(minimal, 0x04, 1);
    put_u32(minimal, 0x10, 1);
    const a2fo::FpqPathResult parsed =
        a2fo::parse_fpq_odf_directories(minimal);
    assert(parsed.ok);
    assert(parsed.version == 1);
    assert(parsed.file_count == 0);
    assert(parsed.odf_directories.empty());

    minimal[0] = 'X';
    assert(!a2fo::parse_fpq_odf_directories(minimal).ok);
}
