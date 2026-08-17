#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo::instant_action_settings {

struct SetupDetailsDecodeResult {
    bool decoded = false;
    std::size_t next_line_offset = 0;
};

SetupDetailsDecodeResult decode_setup_details_line(
    const char* text, std::size_t text_size, std::uint8_t* output,
    std::size_t output_size) noexcept;

}  // namespace a2fo::instant_action_settings
