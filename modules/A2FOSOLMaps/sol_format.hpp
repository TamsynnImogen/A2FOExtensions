#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::sol {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct StartLocation {
    Vec3 position{};
    std::string type;
};

struct Object {
    std::string odf;
    std::uint32_t id = 0;
    Vec3 position{};
    std::int32_t team = 0;
    std::string instance_name;
    std::uint32_t opaque_id = 0;
    std::array<float, 12> transform{};
};

struct Map {
    std::uint32_t version = 0;
    std::string game_type;
    std::string guid;
    std::string title;
    std::string source_path;
    std::string filename;
    std::string mission_script;
    Vec3 minimum_extent{};
    Vec3 size{};
    // Absolute file offsets (from byte zero of the SOL) for runtime bridge
    // boundaries discovered while parsing the tagged stream.  Zero means the
    // boundary was not found/validated.
    std::size_t object_count_record_offset = 0;
    std::size_t runtime_mission_record_offset = 0;
    std::vector<StartLocation> start_locations;
    std::vector<Object> objects;
};

bool parse_bytes(const std::vector<std::uint8_t>& bytes, Map& output,
                 std::string* error = nullptr);
bool parse_file(const std::string& path, Map& output,
                std::string* error = nullptr);

}  // namespace a2fo::sol
