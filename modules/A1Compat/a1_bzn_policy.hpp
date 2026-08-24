#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace a1compat {

// Versions represented by the currently available Armada 1 retail and mod
// maps. Keep this deliberately narrow until another A1 version is backed by a
// real sample: the runtime width shim must never activate for an A2 BZN.
constexpr std::uint32_t kMinimumSupportedA1BznVersion = 2050;
constexpr std::uint32_t kMaximumSupportedA1BznVersion = 2053;
// Observed retail/mod A1 version-2053 maps use the same 40-byte RtimeClass
// field as A2. Retain 32 only as a dynamically detected legacy field size;
// never assume it solely from the map version.
constexpr std::size_t kLegacySerializedRtimeClassNameSize = 32;
constexpr std::size_t kA2SerializedRtimeClassNameSize = 40;
constexpr std::size_t kMaximumA1BznMissionNameSize = 63;
// Real A1 maps can place the final labelled map-bound fields beyond byte 512.
// This remains a small, fixed upper bound so header inspection never probes a
// complete multi-megabyte BZN allocation.
constexpr std::size_t kMaximumA1BznHeaderInspectionSize = 4096;
constexpr std::size_t kMaximumA1MdfStartLocations = 8;
constexpr std::uint32_t kA1MdfGridMaximum = 117;
// Armada II's world/scanner volume uses this native vertical envelope. A1's
// serialized Y extents describe the ordinary object/camera band and can omit
// below-grid scenery such as moons (-50) and wormholes (-30).
constexpr float kA2MinimumVerticalExtent = -1250.0f;
constexpr float kA2MaximumVerticalExtent = 1250.0f;

struct A1BznHeader {
    std::uint32_t version = 0;
    std::size_t metadata_offset = 0;
    char mission_name[kMaximumA1BznMissionNameSize + 1]{};
    bool has_map_bounds = false;
    float minimum_extent[3]{};
    float map_size[3]{};
};

struct A1MdfStartLocation {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};

struct A1MdfData {
    std::size_t start_location_count = 0;
    std::array<A1MdfStartLocation, kMaximumA1MdfStartLocations>
        start_locations{};
};

struct A1BznObjectTailLayout {
    std::size_t first_object_offset = 0;
    std::size_t mission_offset = 0;
    std::uint32_t object_count = 0;
};

// Recognize only the Armada 1 front matter. A numeric version alone is not
// enough because an inherited or malformed A2 stream must retain A2's
// 40-byte RtimeClass record contract.
bool parse_a1_bzn_header(const std::uint8_t* data, std::size_t size,
                         A1BznHeader* header = nullptr) noexcept;

// Adapt parsed A1 extents for A2's MapDetails and live world bounds. Preserve
// the legacy X/Z dimensions and any unusually broad source Y range, but never
// narrow A2's scanner volume below its native vertical envelope.
bool a2_compatible_map_bounds(const A1BznHeader& header,
                              float minimum_extent[3],
                              float map_size[3]) noexcept;

// Locate an A1 neutral-object block between the A2 primary-object cursor and
// the serialized EmptyMission record. Some A1 classes leave a serialized tail
// from the final primary object before the first neutral-object prefix, so the
// first object need not begin at offset zero. The locator remains strict: it
// requires one unique mission marker and complete object-prefix signatures.
bool locate_a1_bzn_object_tail(
    const std::uint8_t* data, std::size_t size,
    A1BznObjectTailLayout* layout) noexcept;

// A1 keeps multiplayer start locations in the companion MDF rather than the
// BZN MapDetails records used by A2. Coordinates are minimap-grid positions in
// the inclusive range 0..117.
bool parse_a1_mdf(const char* data, std::size_t size,
                  A1MdfData* mdf) noexcept;

// Translate an A1 MDF minimap coordinate into A2's world-space SLPos. A1's
// minimap Y axis runs downward, hence the Z inversion.
bool a1_mdf_world_position(const A1BznHeader& header,
                           const A1MdfStartLocation& location,
                           float output[3]) noexcept;

}  // namespace a1compat
