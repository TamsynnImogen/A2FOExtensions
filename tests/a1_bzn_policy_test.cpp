#include "a1_bzn_policy.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> make_a1_header(const char* newline = "\r\n",
                                         unsigned version = 2053,
                                         const char* mission = nullptr,
                                         bool map_bounds = false) {
    std::string text = "version [1] =";
    text += newline;
    text += std::to_string(version);
    text += newline;
    text += "saveGameDesc = ";
    text += std::string(256, 'A');
    text += newline;
    text += "binarySave [1] =";
    text += newline;
    text += "true";
    text += newline;
    std::vector<std::uint8_t> result{text.begin(), text.end()};
    if (mission) {
        const std::uint8_t field_marker[4] = {0x02, 0xdd, 0x60, 0x00};
        result.insert(result.end(), std::begin(field_marker),
                      std::end(field_marker));
        constexpr std::uint32_t allocation_size = 16;
        const auto* allocation = reinterpret_cast<const std::uint8_t*>(
            &allocation_size);
        result.insert(result.end(), allocation,
                      allocation + sizeof(allocation_size));
        const std::size_t length = std::char_traits<char>::length(mission);
        assert(length < allocation_size);
        result.insert(result.end(), mission, mission + length);
        result.insert(result.end(), allocation_size - length, 0);
        if (map_bounds) {
            const auto append_field = [&](std::uint8_t type,
                                          const void* value,
                                          std::uint32_t size) {
                const std::uint8_t marker[4] = {type, 0xdd, 0x60, 0x00};
                result.insert(result.end(), std::begin(marker),
                              std::end(marker));
                const auto* size_bytes =
                    reinterpret_cast<const std::uint8_t*>(&size);
                result.insert(result.end(), size_bytes,
                              size_bytes + sizeof(size));
                const auto* bytes =
                    reinterpret_cast<const std::uint8_t*>(value);
                result.insert(result.end(), bytes, bytes + size);
            };
            const std::uint8_t binary = 1;
            append_field(0x01, &binary, sizeof(binary));
            std::uint8_t fixed_name[100]{};
            std::memcpy(fixed_name, mission, length);
            append_field(0x02, fixed_name, sizeof(fixed_name));
            const float extents[6] = {
                0.0f, 6400.0f, -28.5f, 105.5f, 0.0f, 6400.0f};
            for (float extent : extents) {
                append_field(0x05, &extent, sizeof(extent));
            }
        }
    }
    return result;
}

void append_field(std::vector<std::uint8_t>& data, std::uint8_t type,
                  const void* value, std::uint32_t size) {
    // Real A1 records retain non-zero diagnostic bytes above the low-byte
    // field type. The compatibility parser must ignore those upper bytes.
    const std::uint32_t marker = 0x8174f000u | type;
    const auto* marker_bytes = reinterpret_cast<const std::uint8_t*>(&marker);
    data.insert(data.end(), marker_bytes, marker_bytes + sizeof(marker));
    const auto* size_bytes = reinterpret_cast<const std::uint8_t*>(&size);
    data.insert(data.end(), size_bytes, size_bytes + sizeof(size));
    const auto* value_bytes = static_cast<const std::uint8_t*>(value);
    data.insert(data.end(), value_bytes, value_bytes + size);
}

void append_object_prefix(std::vector<std::uint8_t>& data,
                          const char* odf_name, const char* unique_name) {
    std::uint8_t odf[8]{};
    std::memcpy(odf, odf_name, std::strlen(odf_name));
    append_field(data, 7, odf, sizeof(odf));
    const std::uint32_t flags = 0;
    append_field(data, 4, &flags, sizeof(flags));
    const float position[3]{};
    append_field(data, 9, position, sizeof(position));
    const std::uint32_t team = 0;
    append_field(data, 4, &team, sizeof(team));
    std::uint8_t name[40]{};
    std::memcpy(name, unique_name, std::strlen(unique_name));
    append_field(data, 2, name, sizeof(name));
}

void append_empty_mission(std::vector<std::uint8_t>& data) {
    std::uint8_t name[40]{};
    std::memcpy(name, "EmptyMission", sizeof("EmptyMission"));
    append_field(data, 2, name, sizeof(name));
}

}  // namespace

int main() {
    for (unsigned version = a1compat::kMinimumSupportedA1BznVersion;
         version <= a1compat::kMaximumSupportedA1BznVersion; ++version) {
        const std::vector<std::uint8_t> data = make_a1_header("\r\n", version);
        a1compat::A1BznHeader header;
        assert(a1compat::parse_a1_bzn_header(
            data.data(), data.size(), &header));
        assert(header.version == version);
        assert(header.metadata_offset == data.size());
        assert(header.mission_name[0] == '\0');
    }

    const std::vector<std::uint8_t> mission =
        make_a1_header("\r\n", 2053, "jmult1.bzn");
    a1compat::A1BznHeader mission_header;
    assert(a1compat::parse_a1_bzn_header(
        mission.data(), mission.size(), &mission_header));
    assert(std::string(mission_header.mission_name) == "jmult1.bzn");
    assert(!mission_header.has_map_bounds);

    const std::vector<std::uint8_t> bounded =
        make_a1_header("\r\n", 2053, "jmult1.bzn", true);
    // The six labelled extent fields in real A1 headers cross the historical
    // 512-byte inspection boundary. A truncated probe still identifies the
    // A1 header but cannot provide bounds; the configured bounded read must.
    assert(bounded.size() > 512);
    assert(bounded.size() <=
           a1compat::kMaximumA1BznHeaderInspectionSize);
    a1compat::A1BznHeader truncated_header;
    assert(a1compat::parse_a1_bzn_header(
        bounded.data(), 512, &truncated_header));
    assert(!truncated_header.has_map_bounds);
    a1compat::A1BznHeader bounded_header;
    assert(a1compat::parse_a1_bzn_header(
        bounded.data(), bounded.size(), &bounded_header));
    assert(bounded_header.has_map_bounds);
    assert(bounded_header.minimum_extent[0] == 0.0f);
    assert(bounded_header.minimum_extent[1] == -28.5f);
    assert(bounded_header.minimum_extent[2] == 0.0f);
    assert(bounded_header.map_size[0] == 6400.0f);
    assert(bounded_header.map_size[1] == 134.0f);
    assert(bounded_header.map_size[2] == 6400.0f);

    float compatible_minimum[3]{};
    float compatible_size[3]{};
    assert(a1compat::a2_compatible_map_bounds(
        bounded_header, compatible_minimum, compatible_size));
    assert(compatible_minimum[0] == 0.0f);
    assert(compatible_minimum[1] == -1250.0f);
    assert(compatible_minimum[2] == 0.0f);
    assert(compatible_size[0] == 6400.0f);
    assert(compatible_size[1] == 2500.0f);
    assert(compatible_size[2] == 6400.0f);

    std::vector<std::uint8_t> invalid_mission = mission;
    invalid_mission[mission_header.metadata_offset + 8] = '!';
    a1compat::A1BznHeader invalid_mission_header;
    assert(a1compat::parse_a1_bzn_header(
        invalid_mission.data(), invalid_mission.size(),
        &invalid_mission_header));
    assert(invalid_mission_header.mission_name[0] == '\0');

    const std::vector<std::uint8_t> lf = make_a1_header("\n", 2050);
    assert(a1compat::parse_a1_bzn_header(lf.data(), lf.size()));

    std::vector<std::uint8_t> invalid_hex = make_a1_header();
    invalid_hex[invalid_hex.size() / 2] = 'Z';
    assert(!a1compat::parse_a1_bzn_header(
        invalid_hex.data(), invalid_hex.size()));

    const std::vector<std::uint8_t> a2 = [] {
        const std::string text =
            "version [1] =\r\n2172\r\nBinaryMode [1] =\r\ntrue\r\n";
        return std::vector<std::uint8_t>(text.begin(), text.end());
    }();
    assert(!a1compat::parse_a1_bzn_header(a2.data(), a2.size()));

    const std::vector<std::uint8_t> future = make_a1_header("\r\n", 2067);
    assert(!a1compat::parse_a1_bzn_header(future.data(), future.size()));

    const std::vector<std::uint8_t> truncated = make_a1_header();
    assert(!a1compat::parse_a1_bzn_header(
        truncated.data(), truncated.size() - 1));

    assert(a1compat::kLegacySerializedRtimeClassNameSize == 32);
    assert(a1compat::kA2SerializedRtimeClassNameSize == 40);

    std::vector<std::uint8_t> object_tail(280, 0xaa);
    append_object_prefix(object_tail, "mnebula", "mnebula50");
    append_object_prefix(object_tail, "mdmoon", "mdmoon1");
    const std::size_t expected_mission_offset = object_tail.size();
    append_empty_mission(object_tail);
    a1compat::A1BznObjectTailLayout tail_layout;
    assert(a1compat::locate_a1_bzn_object_tail(
        object_tail.data(), object_tail.size(), &tail_layout));
    assert(tail_layout.first_object_offset == 280);
    assert(tail_layout.mission_offset == expected_mission_offset);
    assert(tail_layout.object_count == 2);

    std::vector<std::uint8_t> ambiguous_tail = object_tail;
    append_empty_mission(ambiguous_tail);
    assert(!a1compat::locate_a1_bzn_object_tail(
        ambiguous_tail.data(), ambiguous_tail.size(), &tail_layout));

    const std::string mdf_text =
        "StartLocations = 2\r\n"
        "Start1 = 109 110\r\n"
        "Start2 = 8 9\r\n";
    a1compat::A1MdfData mdf;
    assert(a1compat::parse_a1_mdf(
        mdf_text.data(), mdf_text.size(), &mdf));
    assert(mdf.start_location_count == 2);
    assert(mdf.start_locations[0].x == 109);
    assert(mdf.start_locations[0].y == 110);
    assert(mdf.start_locations[1].x == 8);
    assert(mdf.start_locations[1].y == 9);

    float first_start[3]{};
    assert(a1compat::a1_mdf_world_position(
        bounded_header, mdf.start_locations[0], first_start));
    assert(std::fabs(first_start[0] - (6400.0f * 109.0f / 117.0f)) <
           0.001f);
    assert(std::fabs(first_start[1] - 38.5f) < 0.001f);
    assert(std::fabs(first_start[2] - (6400.0f * 7.0f / 117.0f)) <
           0.001f);

    const std::string lf_mdf =
        "  StartLocations=1\nStart1=0 117\n";
    assert(a1compat::parse_a1_mdf(
        lf_mdf.data(), lf_mdf.size(), &mdf));
    assert(mdf.start_location_count == 1);

    const std::string missing_start =
        "StartLocations = 2\nStart1 = 10 10\n";
    assert(!a1compat::parse_a1_mdf(
        missing_start.data(), missing_start.size(), &mdf));
    const std::string invalid_grid =
        "StartLocations = 1\nStart1 = 118 0\n";
    assert(!a1compat::parse_a1_mdf(
        invalid_grid.data(), invalid_grid.size(), &mdf));
    return 0;
}
