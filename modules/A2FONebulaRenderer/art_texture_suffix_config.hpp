/*
 * Host-testable ART_CFG.h texture-suffix controls.
 */

#pragma once

#include <string>
#include <string_view>

namespace a2fo::nebula {

struct ArtTextureSuffixConfig {
    // Appended to the diffuse basename before the lowercase subsystem token.
    // For example, "fbattle" + "_emissive_" + "warp".
    std::string emissive_suffix;

    // Appended to the diffuse basename to locate a native DOT3 bump map.
    std::string bump_suffix;

    // Appended to the diffuse basename to locate a specular intensity map.
    std::string specular_suffix;

    // Scales emissive RGB only in the combined DOT3 bump/emissive shader.
    float emissive_bump_multiplier = 1.0f;

    // Added to the DOT3 light result before it modulates the diffuse map.
    float bump_light_bias = 0.2f;

    // Restores unlit diffuse colour beneath non-black emissive pixels.
    float emissive_diffuse_restore = 0.0f;
};

struct ArtTextureSuffixParseReport {
    unsigned valid_assignments = 0;
    unsigned invalid_assignments = 0;
    bool emissive_suffix_found = false;
    bool bump_suffix_found = false;
    bool specular_suffix_found = false;
    bool emissive_bump_multiplier_found = false;
    bool bump_light_bias_found = false;
    bool emissive_diffuse_restore_found = false;
};

// Recognizes either preprocessor definitions or ordinary quoted assignments:
//   #define A2FO_EMISSIVE_SUFFIX "_emissive_"
//   #define A2FO_BUMP_SUFFIX "_bump"
//   #define A2FO_SPECULAR_SUFFIX "_specular"
//   #define A2FO_EMISSIVE_BUMP_MULTIPLIER 2.0
//   #define A2FO_BUMP_LIGHT_BIAS 0.55
//   #define A2FO_EMISSIVE_DIFFUSE_RESTORE 1.0
// Calling this repeatedly in extension-root order gives child mods normal
// override behavior. An empty quoted value disables the corresponding rule.
ArtTextureSuffixParseReport parse_art_texture_suffix_config(
    std::string_view source, ArtTextureSuffixConfig* config);

// Inserts suffix before a final file extension, if present.
std::string texture_name_with_suffix(
    std::string_view diffuse_name, std::string_view suffix);

// Applies the global emissive convention. System tokens are supplied by the
// renderer (warp, impulse, shields, life, sensor, weapons).
std::string emissive_texture_name(
    std::string_view diffuse_name, std::string_view emissive_suffix,
    std::string_view system_token);

}  // namespace a2fo::nebula
