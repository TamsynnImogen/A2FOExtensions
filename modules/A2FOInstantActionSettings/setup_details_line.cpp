#include "setup_details_line.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace a2fo::instant_action_settings {
namespace {

constexpr char kSetupDetailsLabel[] = "setupDetails";

int hex_value(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool horizontal_space(char value) noexcept {
    return value == ' ' || value == '\t';
}

}  // namespace

SetupDetailsDecodeResult decode_setup_details_line(
    const char* text, std::size_t text_size, std::uint8_t* output,
    std::size_t output_size) noexcept {
    SetupDetailsDecodeResult result{};
    if (!text || !output || output_size == 0) return result;

    std::size_t line_end = 0;
    while (line_end < text_size && text[line_end] != '\r' &&
           text[line_end] != '\n' && text[line_end] != '\0') {
        ++line_end;
    }
    if (line_end == text_size) return result;

    std::size_t cursor = 0;
    while (cursor < line_end && horizontal_space(text[cursor])) ++cursor;
    const std::size_t label_size = sizeof(kSetupDetailsLabel) - 1;
    if (line_end - cursor < label_size ||
        std::memcmp(text + cursor, kSetupDetailsLabel, label_size) != 0) {
        return result;
    }
    cursor += label_size;
    while (cursor < line_end && horizontal_space(text[cursor])) ++cursor;
    if (cursor == line_end || text[cursor] != '=') return result;
    ++cursor;
    while (cursor < line_end && horizontal_space(text[cursor])) ++cursor;

    if (output_size > (line_end - cursor) / 2) return result;
    for (std::size_t index = 0; index < output_size; ++index) {
        const int high = hex_value(text[cursor + index * 2]);
        const int low = hex_value(text[cursor + index * 2 + 1]);
        if (high < 0 || low < 0) return result;
        output[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    cursor += output_size * 2;
    while (cursor < line_end && horizontal_space(text[cursor])) ++cursor;
    if (cursor != line_end) return result;

    std::size_t next = line_end;
    if (next < text_size && text[next] == '\r') ++next;
    if (next < text_size && text[next] == '\n') ++next;
    result.decoded = true;
    result.next_line_offset = next;
    return result;
}

}  // namespace a2fo::instant_action_settings
