#include "m3d_conversion.hpp"
#include "../../rust/m3d_ffi.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>

namespace a2fo::m3d {
namespace {

struct Context {
    std::vector<std::uint8_t>& output;
    ConversionStats& stats;
    std::string& error;
    std::exception_ptr callback_exception;
};

std::string span_string(const std::uint8_t* data, std::size_t size) {
    return size ? std::string(reinterpret_cast<const char*>(data), size)
                : std::string{};
}

int emit(void* raw, const A2FoM3DEvent* event) noexcept {
    auto& context = *static_cast<Context*>(raw);
    try {
        const auto& v = event->values;
        switch (event->kind) {
        case A2FO_M3D_STATS: {
            auto& s = context.stats;
            s.source_version = event->bounds_min[0];
            s.materials = v[0];
            s.source_nodes = v[1];
            s.output_nodes = v[2];
            s.meshes = v[3];
            s.groups = v[4];
            s.triangles = v[5];
            s.animations = v[6];
            s.texture_references = v[7];
            s.source_roots = v[8];
            s.synthesized_root = v[9] != 0;
            s.reordered_nodes = v[10];
            s.disambiguated_node_names = v[11];
            s.damage_emitters_adapted = v[12];
            s.sod_materials_end = v[13];
            s.sod_nodes_end = v[14];
            s.sod_animations_end = v[15];
            s.sod_references_end = v[16];
            s.legacy_sod_passthrough = v[17] != 0;
            break;
        }
        case A2FO_M3D_WARNING:
            context.stats.warnings.push_back(
                span_string(event->first, event->first_length));
            break;
        case A2FO_M3D_HIERARCHY:
            context.stats.hierarchy_bindings.push_back({
                span_string(event->first, event->first_length),
                span_string(event->second, event->second_length),
                span_string(event->third, event->third_length)});
            break;
        case A2FO_M3D_TEXTURE: {
            RuntimeTextureBinding binding;
            binding.mesh_name = span_string(event->first, event->first_length);
            binding.diffuse_texture = span_string(event->second, event->second_length);
            binding.bump_texture = span_string(event->third, event->third_length);
            std::memcpy(binding.local_bounds_min.data(), event->bounds_min,
                        sizeof(event->bounds_min));
            std::memcpy(binding.local_bounds_max.data(), event->bounds_max,
                        sizeof(event->bounds_max));
            binding.has_local_bounds = v[0] != 0;
            context.stats.texture_bindings.push_back(std::move(binding));
            break;
        }
        case A2FO_M3D_OUTPUT:
            if (event->first_length) {
                context.output.assign(event->first,
                                      event->first + event->first_length);
            }
            break;
        case A2FO_M3D_ERROR:
            context.error = span_string(event->first, event->first_length);
            break;
        default:
            return 0;
        }
        return 1;
    } catch (...) {
        context.callback_exception = std::current_exception();
        return 0;
    }
}

std::size_t format_version(void* raw, std::uint32_t bits,
                           char* destination, std::size_t capacity) noexcept {
    auto& context = *static_cast<Context*>(raw);
    try {
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        std::ostringstream stream;
        stream << value;
        const std::string formatted = stream.str();
        const std::size_t length = std::min(capacity, formatted.size());
        if (length) std::memcpy(destination, formatted.data(), length);
        return length;
    } catch (...) {
        context.callback_exception = std::current_exception();
        return 0;
    }
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& bytes,
               std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not open M3D file: " + path;
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        error = "Could not determine M3D file size: " + path;
        return false;
    }
    input.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));
    if (size != 0 &&
        !input.read(reinterpret_cast<char*>(bytes.data()), size)) {
        error = "Could not read complete M3D file: " + path;
        return false;
    }
    return true;
}

bool write_file(const std::string& path,
                const std::vector<std::uint8_t>& bytes,
                std::string& error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not create generated SOD: " + path;
        return false;
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output) {
        error = "Could not write complete generated SOD: " + path;
        return false;
    }
    return true;
}

}  // namespace

bool convert_bytes_to_sod(const std::vector<std::uint8_t>& input,
                          std::vector<std::uint8_t>& output,
                          ConversionStats& stats,
                          std::string& error) {
    output.clear();
    stats = {};
    error.clear();
    Context context{output, stats, error, {}};
    const bool success = a2fo_rs_m3d_convert(input.data(), input.size(),
                                              &context, emit, format_version) != 0;
    if (context.callback_exception) std::rethrow_exception(context.callback_exception);
    return success;
}

bool convert_file_to_sod(const std::string& source_path,
                         const std::string& destination_path,
                         ConversionStats& stats,
                         std::string& error) {
    std::vector<std::uint8_t> input;
    if (!read_file(source_path, input, error)) return false;
    std::vector<std::uint8_t> output;
    if (!convert_bytes_to_sod(input, output, stats, error)) return false;
    return write_file(destination_path, output, error);
}

}  // namespace a2fo::m3d
