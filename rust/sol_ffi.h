#pragma once

#include <cstddef>
#include <cstdint>

// Private synchronous ABI. The callback may read these spans only during the call.
extern "C" {
struct A2FoSolEvent {
    std::uint32_t kind;
    std::size_t value;
    const std::uint8_t* data;
    std::size_t length;
    const std::uint8_t* extra;
    std::size_t extra_length;
    std::uint32_t position[3];
    std::uint32_t transform[12];
    std::uint32_t id;
    std::uint32_t team;
    std::uint32_t opaque_id;
};
using A2FoSolEmit = int (*)(void*, const A2FoSolEvent*) noexcept;
using A2FoSolVersion = int (*)(void*, const std::uint8_t*, std::size_t,
                               std::uint32_t*) noexcept;
int a2fo_rs_sol_parse(const std::uint8_t*, std::size_t, void*,
                      A2FoSolEmit, A2FoSolVersion) noexcept;
}

enum : std::uint32_t {
    A2FO_SOL_ERROR = 0,
    A2FO_SOL_VERSION = 1,
    A2FO_SOL_GAME_TYPE = 2,
    A2FO_SOL_GUID = 3,
    A2FO_SOL_TITLE = 4,
    A2FO_SOL_SOURCE_PATH = 5,
    A2FO_SOL_FILENAME = 6,
    A2FO_SOL_MIN_EXTENT = 7,
    A2FO_SOL_SIZE = 8,
    A2FO_SOL_MISSION_SCRIPT = 9,
    A2FO_SOL_START_CLEAR = 10,
    A2FO_SOL_START = 11,
    A2FO_SOL_OBJECT_CLEAR = 12,
    A2FO_SOL_OBJECT = 13,
    A2FO_SOL_RUNTIME_OFFSET = 14,
};
