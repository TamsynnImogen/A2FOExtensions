#include "sol_format.hpp"
#include "../../rust/sol_ffi.h"
#include "../../rust/stage9_sol_completion_ffi.h"

#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace a2fo::sol {
namespace {

struct ParseContext {
    Map* output;
    std::string* error;
    std::exception_ptr exception;
};

float as_float(std::uint32_t bits) noexcept {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string as_string(const std::uint8_t* bytes, std::size_t length) {
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

int emit(void* opaque, const A2FoSolEvent* event) noexcept {
    auto& context = *static_cast<ParseContext*>(opaque);
    Map& map = *context.output;
    try {
        switch (event->kind) {
        case A2FO_SOL_ERROR:
            if (context.error) *context.error = as_string(event->data, event->length);
            break;
        case A2FO_SOL_VERSION: map.version = static_cast<std::uint32_t>(event->value); break;
        case A2FO_SOL_GAME_TYPE: map.game_type = as_string(event->data, event->length); break;
        case A2FO_SOL_GUID: map.guid = as_string(event->data, event->length); break;
        case A2FO_SOL_TITLE: map.title = as_string(event->data, event->length); break;
        case A2FO_SOL_SOURCE_PATH: map.source_path = as_string(event->data, event->length); break;
        case A2FO_SOL_FILENAME: map.filename = as_string(event->data, event->length); break;
        case A2FO_SOL_MIN_EXTENT:
            map.minimum_extent = {as_float(event->position[0]), as_float(event->position[1]),
                                  as_float(event->position[2])};
            break;
        case A2FO_SOL_SIZE:
            map.size = {as_float(event->position[0]), as_float(event->position[1]),
                        as_float(event->position[2])};
            break;
        case A2FO_SOL_MISSION_SCRIPT:
            map.mission_script = as_string(event->data, event->length);
            break;
        case A2FO_SOL_START_CLEAR:
            map.start_locations.clear();
            map.start_locations.reserve(event->value);
            break;
        case A2FO_SOL_START: {
            StartLocation start;
            start.position = {as_float(event->position[0]), as_float(event->position[1]),
                              as_float(event->position[2])};
            start.type = as_string(event->data, event->length);
            map.start_locations.push_back(std::move(start));
            break;
        }
        case A2FO_SOL_OBJECT_CLEAR:
            map.object_count_record_offset = event->value;
            map.objects.clear();
            map.objects.reserve(event->id);
            break;
        case A2FO_SOL_OBJECT: {
            Object object;
            object.odf = as_string(event->data, event->length);
            object.id = event->id;
            object.position = {as_float(event->position[0]), as_float(event->position[1]),
                               as_float(event->position[2])};
            object.team = static_cast<std::int32_t>(event->team);
            object.instance_name = as_string(event->extra, event->extra_length);
            object.opaque_id = event->opaque_id;
            for (std::size_t i = 0; i < object.transform.size(); ++i)
                object.transform[i] = as_float(event->transform[i]);
            map.objects.push_back(std::move(object));
            break;
        }
        case A2FO_SOL_RUNTIME_OFFSET:
            map.runtime_mission_record_offset = event->value;
            break;
        default: return 0;
        }
        return 1;
    } catch (...) {
        context.exception = std::current_exception();
        return 0;
    }
}

int parse_version(void*, const std::uint8_t* data, std::size_t length,
                  std::uint32_t* value) noexcept {
    try {
        const unsigned long parsed = std::stoul(as_string(data, length));
        if (!a2fo_rs_stage9_sol_version_fits(parsed)) return 0;
        *value = static_cast<std::uint32_t>(parsed);
        return 1;
    } catch (...) {
        return 0;
    }
}

}  // namespace

bool parse_bytes(const std::vector<std::uint8_t>& bytes, Map& output,
                 std::string* error) {
    output = Map{};
    ParseContext context{&output, error, {}};
    const int result = a2fo_rs_sol_parse(bytes.data(), bytes.size(), &context,
                                         emit, parse_version);
    if (context.exception) std::rethrow_exception(context.exception);
    return result != 0;
}

bool parse_file(const std::string& path, Map& output, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "unable to open SOL file: " + path;
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (!a2fo_rs_stage9_sol_file_size(end)) {
        if (error) *error = "SOL file size is invalid or unsupported";
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.read(reinterpret_cast<char*>(bytes.data()), end);
    if (!input) {
        if (error) *error = "unable to read complete SOL file: " + path;
        return false;
    }
    return parse_bytes(bytes, output, error);
}

}  // namespace a2fo::sol
