#pragma once

#include <cstdint>

namespace armadal::entity_registry {

using ReadWord = std::uint32_t (*)(void*, std::uint32_t);
using ReadBlock = bool (*)(void*, std::uint32_t, void*, std::uint32_t);
using WriteWord = void (*)(void*, std::uint32_t, std::uint32_t);
using WriteBlock = bool (*)(void*, std::uint32_t, const void*, std::uint32_t);

struct Memory {
    void* opaque{};
    ReadWord read_word{};
    ReadBlock read_block{};
    WriteWord write_word{};
    WriteBlock write_block{};

    std::uint32_t read(std::uint32_t address) const {
        return read_word ? read_word(opaque, address) : 0;
    }

    void write(std::uint32_t address, std::uint32_t value) const {
        if (write_word) write_word(opaque, address, value);
    }
};

} // namespace armadal::entity_registry
