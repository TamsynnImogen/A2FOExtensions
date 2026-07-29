#include "fpq_paths.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}
std::vector<std::uint8_t> fixture() {
    constexpr std::size_t header = 0x1c;
    constexpr std::size_t hashes = 2 * 12;
    constexpr std::size_t folders = 2 * 12;
    constexpr std::size_t files = 25;
    const std::string names = "odfcustomunit.odf";
    const std::size_t folder_table = header + hashes;
    const std::size_t file_table = folder_table + folders;
    const std::size_t name_table = file_table + files;
    std::vector<std::uint8_t> bytes(name_table + names.size(), 0);
    bytes[0] = 'F'; bytes[1] = 'P'; bytes[2] = 'Q';
    put_u32(bytes, 4, 2);
    put_u32(bytes, 8, 2);
    put_u32(bytes, 12, 1);
    put_u32(bytes, 16, 2);
    put_u32(bytes, 24, static_cast<std::uint32_t>(names.size()));
    put_u32(bytes, folder_table, 0xffffffffu);
    put_u32(bytes, folder_table + 4, 0);
    put_u32(bytes, folder_table + 8, 3);
    put_u32(bytes, folder_table + 12, 0);
    put_u32(bytes, folder_table + 16, 3);
    put_u32(bytes, folder_table + 20, 6);
    put_u32(bytes, file_table + 12, 9);
    put_u32(bytes, file_table + 16, 8);
    put_u32(bytes, file_table + 21, 1);
    std::copy(names.begin(), names.end(), bytes.begin() + name_table);
    return bytes;
}

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}  // namespace

int main(int argc, char** argv) {
    const auto synthetic = a2fo::parse_fpq_odf_directories(fixture());
    if (!synthetic.ok || synthetic.odf_directories.size() != 1 ||
        synthetic.odf_directories.front() != "odf\\custom") {
        std::cerr << "synthetic FPQ test failed: " << synthetic.error << '\n';
        return 1;
    }

    for (int index = 1; index < argc; ++index) {
        const auto bytes = read_file(argv[index]);
        const auto parsed = a2fo::parse_fpq_odf_directories(bytes);
        if (!parsed.ok || parsed.file_count == 0 || parsed.odf_directories.empty()) {
            std::cerr << "real FPQ test failed for " << argv[index] << ": "
                      << parsed.error << '\n';
            return 1;
        }
        std::cout << argv[index] << ": " << parsed.file_count << " files, "
                  << parsed.odf_directories.size() << " ODF directories\n";
    }
    return 0;
}
