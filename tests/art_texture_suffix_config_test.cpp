#include "../modules/A2FONebulaRenderer/art_texture_suffix_config.hpp"

#include <cassert>
#include <cmath>

int main() {
    using a2fo::nebula::ArtTextureSuffixConfig;
    using a2fo::nebula::emissive_texture_name;
    using a2fo::nebula::parse_art_texture_suffix_config;
    using a2fo::nebula::texture_name_with_suffix;

    ArtTextureSuffixConfig config;
    auto report = parse_art_texture_suffix_config(R"(
        // #define A2FO_EMISSIVE_SUFFIX "ignored"
        #define A2FO_EMISSIVE_SUFFIX "_emissive_"
        #define A2FO_BUMP_SUFFIX "_bump"
        #define A2FO_SPECULAR_SUFFIX "_specular"
        #define A2FO_EMISSIVE_BUMP_MULTIPLIER 2.0
        #define A2FO_BUMP_LIGHT_BIAS 0.55
        #define A2FO_EMISSIVE_DIFFUSE_RESTORE 1.0
    )", &config);
    assert(report.valid_assignments == 6);
    assert(report.invalid_assignments == 0);
    assert(report.emissive_suffix_found);
    assert(report.bump_suffix_found);
    assert(report.specular_suffix_found);
    assert(report.emissive_bump_multiplier_found);
    assert(report.bump_light_bias_found);
    assert(report.emissive_diffuse_restore_found);
    assert(config.emissive_suffix == "_emissive_");
    assert(config.bump_suffix == "_bump");
    assert(config.specular_suffix == "_specular");
    assert(std::fabs(config.emissive_bump_multiplier - 2.0f) < 0.001f);
    assert(std::fabs(config.bump_light_bias - 0.55f) < 0.001f);
    assert(std::fabs(config.emissive_diffuse_restore - 1.0f) < 0.001f);

    report = parse_art_texture_suffix_config(R"(
        const char* emissiveTextureSuffix = "_glow_";
        const char* bumpTextureSuffix = "";
        const char* specularTextureSuffix = "_shine";
        float emissiveBumpMultiplier = 1.5f;
        float bumpLightBias = 0.4f;
        float emissiveDiffuseRestore = 0.75f;
    )", &config);
    assert(report.valid_assignments == 6);
    assert(config.emissive_suffix == "_glow_");
    assert(config.bump_suffix.empty());
    assert(config.specular_suffix == "_shine");
    assert(std::fabs(config.emissive_bump_multiplier - 1.5f) < 0.001f);
    assert(std::fabs(config.bump_light_bias - 0.4f) < 0.001f);
    assert(std::fabs(config.emissive_diffuse_restore - 0.75f) < 0.001f);

    const ArtTextureSuffixConfig retained = config;
    report = parse_art_texture_suffix_config(R"(
        #define A2FO_EMISSIVE_SUFFIX "../bad"
        #define A2FO_BUMP_SUFFIX "bad.ext"
        #define A2FO_SPECULAR_SUFFIX "../bad"
        #define A2FO_EMISSIVE_BUMP_MULTIPLIER 99.0
        #define A2FO_BUMP_LIGHT_BIAS 1.5
        #define A2FO_EMISSIVE_DIFFUSE_RESTORE 3.0
    )", &config);
    assert(report.valid_assignments == 0);
    assert(report.invalid_assignments == 6);
    assert(config.emissive_suffix == retained.emissive_suffix);
    assert(config.bump_suffix == retained.bump_suffix);
    assert(config.specular_suffix == retained.specular_suffix);
    assert(config.emissive_bump_multiplier ==
           retained.emissive_bump_multiplier);
    assert(config.bump_light_bias == retained.bump_light_bias);
    assert(config.emissive_diffuse_restore ==
           retained.emissive_diffuse_restore);

    assert(texture_name_with_suffix("fbattle", "_bump") ==
           "fbattle_bump");
    assert(texture_name_with_suffix("Textures/RGB/Fbattle.tga", "_bump") ==
           "Textures/RGB/Fbattle_bump.tga");
    assert(emissive_texture_name(
               "fbattle.dds", "_emissive_", "warp") ==
           "fbattle_emissive_warp.dds");
    return 0;
}
