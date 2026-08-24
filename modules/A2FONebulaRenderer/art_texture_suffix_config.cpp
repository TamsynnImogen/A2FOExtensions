#include "art_texture_suffix_config.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace a2fo::nebula {
namespace {

constexpr std::size_t kMaximumSuffixLength = 64;
constexpr float kMaximumEmissiveBumpMultiplier = 8.0f;
constexpr float kMaximumBumpLightBias = 1.0f;
constexpr float kMaximumEmissiveDiffuseRestore = 2.0f;

char lower_ascii(char value) noexcept {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)));
}

bool identifier_character(char value) noexcept {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
        value == '_';
}

std::string lower_identifier(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), lower_ascii);
    return result;
}

std::string strip_c_comments(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    bool line_comment = false;
    bool block_comment = false;
    bool quoted = false;
    bool escaped = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char current = input[index];
        const char next = index + 1 < input.size() ? input[index + 1] : '\0';
        if (line_comment) {
            if (current == '\n' || current == '\r') {
                line_comment = false;
                output.push_back(current);
            } else {
                output.push_back(' ');
            }
            continue;
        }
        if (block_comment) {
            if (current == '*' && next == '/') {
                output.append("  ");
                ++index;
                block_comment = false;
            } else {
                output.push_back(
                    current == '\n' || current == '\r' ? current : ' ');
            }
            continue;
        }
        if (!quoted && current == '/' && next == '/') {
            output.append("  ");
            ++index;
            line_comment = true;
            continue;
        }
        if (!quoted && current == '/' && next == '*') {
            output.append("  ");
            ++index;
            block_comment = true;
            continue;
        }
        output.push_back(current);
        if (quoted) {
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                quoted = false;
            }
        } else if (current == '"') {
            quoted = true;
        }
    }
    return output;
}

bool parse_quoted_value(std::string_view source, std::size_t cursor,
                        std::string* value) {
    if (!value) return false;
    while (cursor < source.size() && std::isspace(
               static_cast<unsigned char>(source[cursor]))) {
        ++cursor;
    }
    if (cursor >= source.size() || source[cursor] != '"') return false;
    ++cursor;
    std::string parsed;
    while (cursor < source.size() && source[cursor] != '"') {
        const unsigned char character =
            static_cast<unsigned char>(source[cursor]);
        if (character < 0x20 || character > 0x7e ||
            source[cursor] == '\\') {
            return false;
        }
        parsed.push_back(source[cursor++]);
        if (parsed.size() > kMaximumSuffixLength) return false;
    }
    if (cursor >= source.size() || source[cursor] != '"') return false;
    ++cursor;
    while (cursor < source.size() &&
           source[cursor] != '\n' && source[cursor] != '\r' &&
           source[cursor] != ';') {
        if (!std::isspace(static_cast<unsigned char>(source[cursor]))) {
            return false;
        }
        ++cursor;
    }
    for (const char character : parsed) {
        const unsigned char value_byte =
            static_cast<unsigned char>(character);
        if (!std::isalnum(value_byte) && character != '_' &&
            character != '-') {
            return false;
        }
    }
    *value = std::move(parsed);
    return true;
}

bool parse_float_value(const std::string& source, std::size_t cursor,
                       float minimum, float maximum, float* value) {
    if (!value) return false;
    while (cursor < source.size() && std::isspace(
               static_cast<unsigned char>(source[cursor])) &&
           source[cursor] != '\n' && source[cursor] != '\r') {
        ++cursor;
    }
    if (cursor >= source.size()) return false;

    errno = 0;
    char* end = nullptr;
    const char* begin = source.c_str() + cursor;
    const float parsed = std::strtof(begin, &end);
    if (end == begin || errno == ERANGE || !std::isfinite(parsed) ||
        parsed < minimum || parsed > maximum) {
        return false;
    }
    cursor = static_cast<std::size_t>(end - source.c_str());
    if (cursor < source.size() &&
        (source[cursor] == 'f' || source[cursor] == 'F')) {
        ++cursor;
    }
    while (cursor < source.size() && source[cursor] != '\n' &&
           source[cursor] != '\r' && source[cursor] != ';') {
        if (!std::isspace(static_cast<unsigned char>(source[cursor]))) {
            return false;
        }
        ++cursor;
    }
    *value = parsed;
    return true;
}

enum class Setting {
    none,
    emissive,
    bump,
    specular,
    emissive_bump_multiplier,
    bump_light_bias,
    emissive_diffuse_restore,
};

Setting setting_for_identifier(std::string_view identifier) {
    const std::string lowered = lower_identifier(identifier);
    if (lowered == "a2fo_emissive_suffix" ||
        lowered == "emissivetexturesuffix") {
        return Setting::emissive;
    }
    if (lowered == "a2fo_bump_suffix" ||
        lowered == "bumptexturesuffix") {
        return Setting::bump;
    }
    if (lowered == "a2fo_specular_suffix" ||
        lowered == "speculartexturesuffix") {
        return Setting::specular;
    }
    if (lowered == "a2fo_emissive_bump_multiplier" ||
        lowered == "emissivebumpmultiplier") {
        return Setting::emissive_bump_multiplier;
    }
    if (lowered == "a2fo_bump_light_bias" ||
        lowered == "bumplightbias") {
        return Setting::bump_light_bias;
    }
    if (lowered == "a2fo_emissive_diffuse_restore" ||
        lowered == "emissivediffuserestore") {
        return Setting::emissive_diffuse_restore;
    }
    return Setting::none;
}

}  // namespace

ArtTextureSuffixParseReport parse_art_texture_suffix_config(
    std::string_view source, ArtTextureSuffixConfig* config) {
    ArtTextureSuffixParseReport report{};
    if (!config) {
        report.invalid_assignments = 1;
        return report;
    }

    const std::string stripped = strip_c_comments(source);
    for (std::size_t cursor = 0; cursor < stripped.size();) {
        if (!identifier_character(stripped[cursor]) ||
            std::isdigit(static_cast<unsigned char>(stripped[cursor]))) {
            ++cursor;
            continue;
        }
        const std::size_t begin = cursor++;
        while (cursor < stripped.size() &&
               identifier_character(stripped[cursor])) {
            ++cursor;
        }
        const Setting setting = setting_for_identifier(
            std::string_view(stripped).substr(begin, cursor - begin));
        if (setting == Setting::none) continue;

        std::size_t value_cursor = cursor;
        while (value_cursor < stripped.size() && std::isspace(
                   static_cast<unsigned char>(stripped[value_cursor])) &&
               stripped[value_cursor] != '\n' &&
               stripped[value_cursor] != '\r') {
            ++value_cursor;
        }
        if (value_cursor < stripped.size() &&
            stripped[value_cursor] == '=') {
            ++value_cursor;
        }

        if (setting == Setting::emissive_bump_multiplier) {
            float parsed = 1.0f;
            if (!parse_float_value(
                    stripped, value_cursor, 0.0f,
                    kMaximumEmissiveBumpMultiplier, &parsed)) {
                ++report.invalid_assignments;
                continue;
            }
            config->emissive_bump_multiplier = parsed;
            report.emissive_bump_multiplier_found = true;
        } else if (setting == Setting::bump_light_bias) {
            float parsed = 0.2f;
            if (!parse_float_value(
                    stripped, value_cursor, 0.0f,
                    kMaximumBumpLightBias, &parsed)) {
                ++report.invalid_assignments;
                continue;
            }
            config->bump_light_bias = parsed;
            report.bump_light_bias_found = true;
        } else if (setting == Setting::emissive_diffuse_restore) {
            float parsed = 0.0f;
            if (!parse_float_value(
                    stripped, value_cursor, 0.0f,
                    kMaximumEmissiveDiffuseRestore, &parsed)) {
                ++report.invalid_assignments;
                continue;
            }
            config->emissive_diffuse_restore = parsed;
            report.emissive_diffuse_restore_found = true;
        } else {
            std::string parsed;
            if (!parse_quoted_value(stripped, value_cursor, &parsed)) {
                ++report.invalid_assignments;
                continue;
            }
            if (setting == Setting::emissive) {
                config->emissive_suffix = std::move(parsed);
                report.emissive_suffix_found = true;
            } else if (setting == Setting::bump) {
                config->bump_suffix = std::move(parsed);
                report.bump_suffix_found = true;
            } else {
                config->specular_suffix = std::move(parsed);
                report.specular_suffix_found = true;
            }
        }
        ++report.valid_assignments;
    }
    return report;
}

std::string texture_name_with_suffix(
    std::string_view diffuse_name, std::string_view suffix) {
    std::string result(diffuse_name);
    if (suffix.empty()) return result;
    const std::size_t slash = result.find_last_of("\\/");
    const std::size_t dot = result.find_last_of('.');
    if (dot == std::string::npos ||
        (slash != std::string::npos && dot <= slash + 1)) {
        result.append(suffix.data(), suffix.size());
    } else {
        result.insert(dot, suffix.data(), suffix.size());
    }
    return result;
}

std::string emissive_texture_name(
    std::string_view diffuse_name, std::string_view emissive_suffix,
    std::string_view system_token) {
    std::string combined(emissive_suffix);
    combined.append(system_token.data(), system_token.size());
    return texture_name_with_suffix(diffuse_name, combined);
}

}  // namespace a2fo::nebula
