#include "art_texture_suffix_config.hpp"
#include "../../rust/config_ffi.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <exception>
namespace a2fo::nebula {
namespace {
A2foRsBytes span(std::string_view s) {
    return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
}
struct Output {
    ArtTextureSuffixConfig *config;
    std::exception_ptr error;
};
void receive(void *p, uint32_t kind, size_t, const uint8_t *data, size_t size) noexcept {
    auto &out = *static_cast<Output *>(p);
    if (out.error)
        return;
    try {
        std::string value(reinterpret_cast<const char *>(data), size);
        switch (kind) {
        case 0:
            out.config->emissive_suffix = std::move(value);
            break;
        case 1:
            out.config->bump_suffix = std::move(value);
            break;
        case 2:
            out.config->specular_suffix = std::move(value);
            break;
        }
    } catch (...) {
        out.error = std::current_exception();
    }
}
uint8_t parse_float(void *p, const uint8_t *data, size_t size, size_t *consumed,
                    float *value) noexcept {
    auto &error = *static_cast<std::exception_ptr *>(p);
    if (error)
        return 0;
    try {
        std::string text(reinterpret_cast<const char *>(data), size);
        errno = 0;
        char *end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || errno == ERANGE || !std::isfinite(parsed))
            return 0;
        *consumed = static_cast<size_t>(end - text.c_str());
        *value = parsed;
        return 1;
    } catch (...) {
        error = std::current_exception();
        return 0;
    }
}
std::string suffix_name(std::string_view name, std::string_view suffix) {
    ArtTextureSuffixConfig storage;
    Output out{&storage, {}};
    // The callback writes into the first string field.
    a2fo_rs_config_texture_suffix(span(name), span(suffix), receive, &out);
    if (out.error)
        std::rethrow_exception(out.error);
    return storage.emissive_suffix;
}
} // namespace
ArtTextureSuffixParseReport parse_art_texture_suffix_config(std::string_view source,
                                                            ArtTextureSuffixConfig *config) {
    ArtTextureSuffixParseReport result{};
    if (!config) {
        result.invalid_assignments = 1;
        return result;
    }
    A2foRsArtReport raw{0,
                        0,
                        0,
                        config->emissive_bump_multiplier,
                        config->bump_light_bias,
                        config->emissive_diffuse_restore};
    Output out{config, {}};
    std::exception_ptr float_error;
    a2fo_rs_config_art(span(source), &raw, receive, &out, parse_float, &float_error);
    if (out.error)
        std::rethrow_exception(out.error);
    if (float_error)
        std::rethrow_exception(float_error);
    config->emissive_bump_multiplier = raw.emissive_bump_multiplier;
    config->bump_light_bias = raw.bump_light_bias;
    config->emissive_diffuse_restore = raw.emissive_diffuse_restore;
    result.valid_assignments = raw.valid;
    result.invalid_assignments = raw.invalid;
    result.emissive_suffix_found = (raw.found & 1) != 0;
    result.bump_suffix_found = (raw.found & 2) != 0;
    result.specular_suffix_found = (raw.found & 4) != 0;
    result.emissive_bump_multiplier_found = (raw.found & 8) != 0;
    result.bump_light_bias_found = (raw.found & 16) != 0;
    result.emissive_diffuse_restore_found = (raw.found & 32) != 0;
    return result;
}
bool settings_disable_bump(std::string_view source, bool *setting_found) {
    uint8_t found = 0;
    const bool disabled = a2fo_rs_config_disable_bump(span(source), &found) != 0;
    if (setting_found)
        *setting_found = found != 0;
    return disabled;
}
std::string texture_name_with_suffix(std::string_view diffuse_name, std::string_view suffix) {
    return suffix_name(diffuse_name, suffix);
}
std::string emissive_texture_name(std::string_view diffuse_name, std::string_view emissive_suffix,
                                  std::string_view system_token) {
    std::string combined(emissive_suffix);
    combined.append(system_token.data(), system_token.size());
    return suffix_name(diffuse_name, combined);
}
} // namespace a2fo::nebula
