#include "a1_bzn_policy.hpp"
#include "../../rust/a1_maps_ffi.h"

#include <cstring>
#include <limits>

namespace a1compat {
namespace {
A2foA1MapHeader to_private(const A1BznHeader& source) noexcept {
    A2foA1MapHeader result{};
    result.version = source.version;
    result.metadata_offset = source.metadata_offset;
    std::memcpy(result.mission_name, source.mission_name, sizeof(result.mission_name));
    result.has_map_bounds = source.has_map_bounds ? 1u : 0u;
    std::memcpy(result.minimum_extent, source.minimum_extent, sizeof(result.minimum_extent));
    std::memcpy(result.map_size, source.map_size, sizeof(result.map_size));
    return result;
}
}

bool serialized_a1_mission_record_at(const std::uint8_t* candidate,
                                     const std::uint8_t* stream_end) noexcept {
    if (!candidate || !stream_end) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(candidate);
    const auto end = reinterpret_cast<std::uintptr_t>(stream_end);
    if (end < begin || end - begin > static_cast<std::uintptr_t>(
            std::numeric_limits<std::ptrdiff_t>::max())) return false;
    return a2fo_rs_a1_mission_record_at(candidate, end - begin) != 0;
}

bool parse_a1_bzn_header(const std::uint8_t* data, std::size_t size,
                         A1BznHeader* header) noexcept {
    if (!data || !size || size > static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max())) return false;
    A2foA1MapHeader parsed = header ? to_private(*header) : A2foA1MapHeader{};
    if (!a2fo_rs_a1_parse_header(data, size, header ? &parsed : nullptr)) return false;
    if (header) {
        header->version = parsed.version;
        header->metadata_offset = parsed.metadata_offset;
        std::memcpy(header->mission_name, parsed.mission_name, sizeof(header->mission_name));
        header->has_map_bounds = parsed.has_map_bounds != 0;
        std::memcpy(header->minimum_extent, parsed.minimum_extent, sizeof(header->minimum_extent));
        std::memcpy(header->map_size, parsed.map_size, sizeof(header->map_size));
    }
    return true;
}

bool a2_compatible_map_bounds(const A1BznHeader& header,
                              float minimum_extent[3], float map_size[3]) noexcept {
    if (!minimum_extent || !map_size) return false;
    const auto private_header = to_private(header);
    return a2fo_rs_a1_compatible_bounds(&private_header, minimum_extent, map_size) != 0;
}

bool locate_a1_bzn_object_tail(const std::uint8_t* data, std::size_t size,
                               A1BznObjectTailLayout* layout) noexcept {
    if (!data || !size || !layout || size > static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max())) return false;
    A2foA1MapTail parsed{};
    if (!a2fo_rs_a1_locate_tail(data, size, &parsed)) return false;
    layout->first_object_offset = parsed.first_object_offset;
    layout->mission_offset = parsed.mission_offset;
    layout->object_count = parsed.object_count;
    return true;
}

bool parse_a1_mdf(const char* data, std::size_t size, A1MdfData* mdf) noexcept {
    if (!data || !size || !mdf || size > static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max())) return false;
    A2foA1Mdf parsed{};
    if (!a2fo_rs_a1_parse_mdf(reinterpret_cast<const std::uint8_t*>(data), size, &parsed)) return false;
    mdf->start_location_count = parsed.start_location_count;
    for (std::size_t index = 0; index < kMaximumA1MdfStartLocations; ++index) {
        mdf->start_locations[index] = {parsed.locations[index][0], parsed.locations[index][1]};
    }
    return true;
}

bool a1_mdf_world_position(const A1BznHeader& header,
                           const A1MdfStartLocation& location, float output[3]) noexcept {
    if (!output) return false;
    const auto private_header = to_private(header);
    return a2fo_rs_a1_world_position(&private_header, location.x, location.y, output) != 0;
}
}  // namespace a1compat
