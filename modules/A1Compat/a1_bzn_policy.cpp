#include "a1_bzn_policy.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>

namespace a1compat {
namespace {

bool valid_mission_name_character(std::uint8_t byte) noexcept {
    return (byte >= 'a' && byte <= 'z') ||
           (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' ||
           byte == '.' || byte == '\\' || byte == '/';
}

bool ends_with_bzn(const char* text, std::size_t length) noexcept {
    if (!text || length < 4) return false;
    const auto lower = [](char byte) noexcept {
        return byte >= 'A' && byte <= 'Z'
            ? static_cast<char>(byte - 'A' + 'a')
            : byte;
    };
    return text[length - 4] == '.' && lower(text[length - 3]) == 'b' &&
           lower(text[length - 2]) == 'z' && lower(text[length - 1]) == 'n';
}

void parse_mission_name(const std::uint8_t* data, std::size_t size,
                        std::size_t metadata_offset,
                        A1BznHeader* header) noexcept {
    if (!data || !header || metadata_offset > size ||
        size - metadata_offset < 8 || data[metadata_offset] != 0x02) {
        return;
    }

    // A1's first binary metadata field is a labelled dynamic string. Its
    // four-byte field marker is followed by the allocated byte count and the
    // NUL-terminated mission BZN name used by AiMission::Create.
    std::uint32_t allocation_size = 0;
    std::memcpy(&allocation_size, data + metadata_offset + 4,
                sizeof(allocation_size));
    if (!allocation_size || allocation_size > 256 ||
        allocation_size > size - metadata_offset - 8) {
        return;
    }

    const auto* value = data + metadata_offset + 8;
    std::size_t length = 0;
    while (length < allocation_size && value[length]) {
        if (length >= kMaximumA1BznMissionNameSize ||
            !valid_mission_name_character(value[length])) {
            return;
        }
        ++length;
    }
    if (!length || length == allocation_size ||
        !ends_with_bzn(reinterpret_cast<const char*>(value), length)) {
        return;
    }

    std::memcpy(header->mission_name, value, length);
    header->mission_name[length] = '\0';
}

bool read_serialized_field(const std::uint8_t* data, std::size_t size,
                           std::size_t& cursor, std::uint8_t type,
                           std::size_t expected_size,
                           const std::uint8_t** value = nullptr) noexcept {
    if (!data || cursor > size || size - cursor < 8 ||
        data[cursor] != type) {
        return false;
    }
    std::uint32_t field_size = 0;
    std::memcpy(&field_size, data + cursor + 4, sizeof(field_size));
    if (field_size != expected_size || field_size > size - cursor - 8) {
        return false;
    }
    if (value) *value = data + cursor + 8;
    cursor += 8 + field_size;
    return true;
}

constexpr char kSerializedEmptyMissionName[] = "EmptyMission";
constexpr std::size_t kMaximumObjectTailLeadingResidualSize = 64 * 1024;

bool serialized_empty_mission_at(
    const std::uint8_t* candidate,
    const std::uint8_t* stream_end) noexcept {
    constexpr std::size_t kMissionRecordPrefixSize =
        8 + sizeof(kSerializedEmptyMissionName);
    if (!candidate || !stream_end || stream_end < candidate ||
        static_cast<std::size_t>(stream_end - candidate) <
            kMissionRecordPrefixSize) {
        return false;
    }
    std::uint32_t field_type = 0;
    std::uint32_t field_size = 0;
    std::memcpy(&field_type, candidate, sizeof(field_type));
    std::memcpy(&field_size, candidate + 4, sizeof(field_size));
    return (field_type & 0xffu) == 2u &&
           field_size == kA2SerializedRtimeClassNameSize &&
           std::memcmp(candidate + 8, kSerializedEmptyMissionName,
                       sizeof(kSerializedEmptyMissionName)) == 0;
}

bool serialized_game_object_prefix_at(
    const std::uint8_t* candidate,
    const std::uint8_t* stream_end) noexcept {
    if (!candidate || !stream_end || stream_end < candidate ||
        static_cast<std::size_t>(stream_end - candidate) < 64) {
        return false;
    }
    const auto read_header = [stream_end](
        const std::uint8_t* field, std::uint8_t expected_type,
        std::uint32_t expected_size) noexcept {
        if (!field || stream_end < field ||
            static_cast<std::size_t>(stream_end - field) < 8) {
            return false;
        }
        std::uint32_t type = 0;
        std::uint32_t size = 0;
        std::memcpy(&type, field, sizeof(type));
        std::memcpy(&size, field + 4, sizeof(size));
        return (type & 0xffu) == expected_type && size == expected_size;
    };

    std::uint32_t name_type = 0;
    std::uint32_t name_size = 0;
    std::memcpy(&name_type, candidate, sizeof(name_type));
    std::memcpy(&name_size, candidate + 4, sizeof(name_size));
    if ((name_type & 0xffu) != 7u || name_size == 0 || name_size > 64u ||
        static_cast<std::size_t>(stream_end - candidate) < 8u + name_size) {
        return false;
    }
    bool saw_character = false;
    bool saw_terminator = false;
    for (std::uint32_t index = 0; index < name_size; ++index) {
        const std::uint8_t value = candidate[8 + index];
        if (value == 0) {
            saw_terminator = true;
            continue;
        }
        if (saw_terminator || value < 0x20u || value > 0x7eu) return false;
        saw_character = true;
    }
    if (!saw_character) return false;

    const std::uint8_t* field = candidate + 8 + name_size;
    if (!read_header(field, 4, 4)) return false;
    field += 12;
    if (!read_header(field, 9, 12)) return false;
    field += 20;
    if (!read_header(field, 4, 4)) return false;
    field += 12;
    return read_header(field, 2, kA2SerializedRtimeClassNameSize);
}

void parse_map_bounds(const std::uint8_t* data, std::size_t size,
                      std::size_t metadata_offset,
                      A1BznHeader* header) noexcept {
    if (!data || !header || metadata_offset > size ||
        size - metadata_offset < 8 || data[metadata_offset] != 0x02) {
        return;
    }

    // The A1 metadata front matter is:
    //   dynamic mission filename, bool, fixed 100-byte filename,
    //   minX, maxX, minY, maxY, minZ, maxZ.
    // Every value uses the ordinary labelled-binary field header. These six
    // floats are the source of A1's map extents; A2 stores equivalent values
    // in MapDetails::MPDMinExtent and MPDSize.
    std::uint32_t mission_allocation = 0;
    std::memcpy(&mission_allocation, data + metadata_offset + 4,
                sizeof(mission_allocation));
    if (!mission_allocation || mission_allocation > 256 ||
        mission_allocation > size - metadata_offset - 8) {
        return;
    }
    std::size_t cursor = metadata_offset + 8 + mission_allocation;
    if (!read_serialized_field(data, size, cursor, 0x01, 1)) return;
    if (!read_serialized_field(data, size, cursor, 0x02, 100)) return;

    float bounds[6]{};
    for (float& bound : bounds) {
        const std::uint8_t* value = nullptr;
        if (!read_serialized_field(data, size, cursor, 0x05,
                                   sizeof(float), &value)) {
            return;
        }
        std::memcpy(&bound, value, sizeof(bound));
        if (!std::isfinite(bound)) return;
    }

    const float sizes[3] = {
        bounds[1] - bounds[0],
        bounds[3] - bounds[2],
        bounds[5] - bounds[4]};
    constexpr float kMaximumSupportedExtent = 1000000.0f;
    if (!(sizes[0] > 0.0f) || !(sizes[1] > 0.0f) ||
        !(sizes[2] > 0.0f) || sizes[0] > kMaximumSupportedExtent ||
        sizes[1] > kMaximumSupportedExtent ||
        sizes[2] > kMaximumSupportedExtent) {
        return;
    }

    header->minimum_extent[0] = bounds[0];
    header->minimum_extent[1] = bounds[2];
    header->minimum_extent[2] = bounds[4];
    std::memcpy(header->map_size, sizes, sizeof(sizes));
    header->has_map_bounds = true;
}

class Cursor {
public:
    Cursor(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    bool consume(const char* text) noexcept {
        const std::size_t length = std::strlen(text);
        if (!available(length) ||
            std::memcmp(data_ + offset_, text, length) != 0) {
            return false;
        }
        offset_ += length;
        return true;
    }

    bool newline() noexcept {
        if (!available(1)) return false;
        if (data_[offset_] == '\n') {
            ++offset_;
            return true;
        }
        if (data_[offset_] == '\r' && available(2) &&
            data_[offset_ + 1] == '\n') {
            offset_ += 2;
            return true;
        }
        return false;
    }

    bool decimal(std::uint32_t* value) noexcept {
        if (!value || !available(1)) return false;
        std::uint32_t result = 0;
        std::size_t digits = 0;
        while (available(1)) {
            const std::uint8_t byte = data_[offset_];
            if (byte < '0' || byte > '9') break;
            const std::uint32_t digit = byte - '0';
            if (result > (std::numeric_limits<std::uint32_t>::max() - digit) /
                             10u) {
                return false;
            }
            result = result * 10u + digit;
            ++offset_;
            ++digits;
        }
        if (!digits) return false;
        *value = result;
        return true;
    }

    bool hex(std::size_t count) noexcept {
        if (!available(count)) return false;
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint8_t byte = data_[offset_ + index];
            const bool valid = (byte >= '0' && byte <= '9') ||
                               (byte >= 'a' && byte <= 'f') ||
                               (byte >= 'A' && byte <= 'F');
            if (!valid) return false;
        }
        offset_ += count;
        return true;
    }

    std::size_t offset() const noexcept { return offset_; }

private:
    bool available(std::size_t count) const noexcept {
        return offset_ <= size_ && count <= size_ - offset_;
    }

    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
};

std::string_view trim_ascii(std::string_view value) noexcept {
    const auto whitespace = [](char character) noexcept {
        return character == ' ' || character == '\t' ||
               character == '\r' || character == '\n';
    };
    while (!value.empty() && whitespace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && whitespace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

bool parse_decimal_token(std::string_view& value,
                         std::uint32_t* output) noexcept {
    value = trim_ascii(value);
    if (!output || value.empty() || value.front() < '0' ||
        value.front() > '9') {
        return false;
    }
    std::uint32_t result = 0;
    std::size_t digits = 0;
    while (digits < value.size() && value[digits] >= '0' &&
           value[digits] <= '9') {
        const std::uint32_t digit =
            static_cast<std::uint32_t>(value[digits] - '0');
        if (result >
            (std::numeric_limits<std::uint32_t>::max() - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
        ++digits;
    }
    value.remove_prefix(digits);
    *output = result;
    return true;
}

bool parse_assignment(std::string_view line, std::string_view* key,
                      std::string_view* value) noexcept {
    if (!key || !value) return false;
    line = trim_ascii(line);
    if (line.empty() || line.front() == '#' || line.front() == ';' ||
        (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
        return false;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string_view::npos) return false;
    *key = trim_ascii(line.substr(0, equals));
    *value = trim_ascii(line.substr(equals + 1));
    return !key->empty() && !value->empty();
}

}  // namespace

bool parse_a1_bzn_header(const std::uint8_t* data, std::size_t size,
                         A1BznHeader* header) noexcept {
    if (!data || !size) return false;

    Cursor cursor(data, size);
    std::uint32_t version = 0;
    if (!cursor.consume("version [1] =") || !cursor.newline() ||
        !cursor.decimal(&version) || !cursor.newline() ||
        version < kMinimumSupportedA1BznVersion ||
        version > kMaximumSupportedA1BznVersion ||
        !cursor.consume("saveGameDesc = ") || !cursor.hex(256) ||
        !cursor.newline() || !cursor.consume("binarySave [1] =") ||
        !cursor.newline() || !cursor.consume("true") || !cursor.newline()) {
        return false;
    }

    if (header) {
        header->version = version;
        header->metadata_offset = cursor.offset();
        parse_mission_name(data, size, cursor.offset(), header);
        parse_map_bounds(data, size, cursor.offset(), header);
    }
    return true;
}

bool a2_compatible_map_bounds(const A1BznHeader& header,
                              float minimum_extent[3],
                              float map_size[3]) noexcept {
    if (!header.has_map_bounds || !minimum_extent || !map_size) return false;

    std::memcpy(minimum_extent, header.minimum_extent,
                sizeof(header.minimum_extent));
    std::memcpy(map_size, header.map_size, sizeof(header.map_size));

    const float source_maximum_y =
        header.minimum_extent[1] + header.map_size[1];
    minimum_extent[1] = std::min(
        header.minimum_extent[1], kA2MinimumVerticalExtent);
    const float maximum_y = std::max(
        source_maximum_y, kA2MaximumVerticalExtent);
    map_size[1] = maximum_y - minimum_extent[1];
    return std::isfinite(map_size[1]) && map_size[1] > 0.0f;
}

bool locate_a1_bzn_object_tail(
    const std::uint8_t* data, std::size_t size,
    A1BznObjectTailLayout* layout) noexcept {
    if (!data || !size || !layout) return false;

    A1BznObjectTailLayout located;
    const auto* end = data + size;
    constexpr std::size_t kMissionRecordPrefixSize =
        8 + sizeof(kSerializedEmptyMissionName);

    std::uint32_t mission_markers = 0;
    for (std::size_t offset = 0;
         offset + kMissionRecordPrefixSize <= size; ++offset) {
        if (!serialized_empty_mission_at(data + offset, end)) continue;
        ++mission_markers;
        located.mission_offset = offset;
        if (mission_markers > 1) return false;
    }
    if (mission_markers != 1 || located.mission_offset == 0) return false;

    bool found_first = false;
    for (std::size_t offset = 0; offset < located.mission_offset; ++offset) {
        if (!serialized_game_object_prefix_at(data + offset, end)) continue;
        if (!found_first) {
            located.first_object_offset = offset;
            found_first = true;
        }
        if (++located.object_count > 1024u) return false;
    }
    if (!found_first || !located.object_count) return false;
    if (located.first_object_offset >
        kMaximumObjectTailLeadingResidualSize) {
        return false;
    }

    *layout = located;
    return true;
}

bool parse_a1_mdf(const char* data, std::size_t size,
                  A1MdfData* mdf) noexcept {
    if (!data || !size || !mdf) return false;

    A1MdfData parsed;
    std::array<bool, kMaximumA1MdfStartLocations> found{};
    bool found_count = false;
    std::size_t cursor = 0;
    while (cursor < size) {
        const std::size_t line_start = cursor;
        while (cursor < size && data[cursor] != '\r' &&
               data[cursor] != '\n') {
            const unsigned char byte =
                static_cast<unsigned char>(data[cursor]);
            if ((byte < 0x20 && byte != '\t') || byte > 0x7e) {
                return false;
            }
            ++cursor;
        }
        const std::string_view line(data + line_start, cursor - line_start);
        if (cursor < size && data[cursor] == '\r') ++cursor;
        if (cursor < size && data[cursor] == '\n') ++cursor;

        std::string_view key;
        std::string_view value;
        if (!parse_assignment(line, &key, &value)) continue;

        if (key == "StartLocations") {
            std::uint32_t count = 0;
            if (found_count || !parse_decimal_token(value, &count) ||
                !trim_ascii(value).empty() ||
                count > kMaximumA1MdfStartLocations) {
                return false;
            }
            parsed.start_location_count = count;
            found_count = true;
            continue;
        }

        if (key.size() <= 5 || key.substr(0, 5) != "Start") continue;
        std::string_view index_text = key.substr(5);
        std::uint32_t index = 0;
        if (!parse_decimal_token(index_text, &index) ||
            !trim_ascii(index_text).empty() || index == 0 ||
            index > kMaximumA1MdfStartLocations) {
            return false;
        }
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        if (found[index - 1] || !parse_decimal_token(value, &x) ||
            !parse_decimal_token(value, &y) ||
            !trim_ascii(value).empty() || x > kA1MdfGridMaximum ||
            y > kA1MdfGridMaximum) {
            return false;
        }
        parsed.start_locations[index - 1] = {x, y};
        found[index - 1] = true;
    }

    if (!found_count) return false;
    for (std::size_t index = 0; index < parsed.start_location_count;
         ++index) {
        if (!found[index]) return false;
    }
    *mdf = parsed;
    return true;
}

bool a1_mdf_world_position(const A1BznHeader& header,
                           const A1MdfStartLocation& location,
                           float output[3]) noexcept {
    if (!header.has_map_bounds || !output ||
        location.x > kA1MdfGridMaximum ||
        location.y > kA1MdfGridMaximum) {
        return false;
    }
    constexpr float grid = static_cast<float>(kA1MdfGridMaximum);
    output[0] = header.minimum_extent[0] +
        header.map_size[0] * (static_cast<float>(location.x) / grid);
    output[1] = header.minimum_extent[1] + header.map_size[1] * 0.5f;
    output[2] = header.minimum_extent[2] +
        header.map_size[2] *
            (static_cast<float>(kA1MdfGridMaximum - location.y) / grid);
    return std::isfinite(output[0]) && std::isfinite(output[1]) &&
           std::isfinite(output[2]);
}

}  // namespace a1compat
