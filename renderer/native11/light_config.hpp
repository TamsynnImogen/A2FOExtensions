#pragma once
#include "scene.hpp"
#include <array>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct LightField { std::string name, value; };
struct LightConfig {
    bool enabled=false;
    Light light{};
    bool cast_shadows=true;
    bool legacy_native_source=false;
};

// Names to request from the copied class-ODF event. Unknown input fields are ignored.
const std::array<const char*,20>& requested_light_fields() noexcept;
// On failure, output is unchanged. No engine-owned pointers are retained.
bool parse_light_config(const std::vector<LightField>& fields,LightConfig& output,std::string& error);
} // namespace a2fo::native11
