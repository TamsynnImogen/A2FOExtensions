#include "../renderer/refit/material_colours.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace a2fo::refit;

namespace {
int failures = 0;
void expect(bool condition, const char* label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
bool equal3(const float* a, const float* b) {
    for (unsigned i = 0; i < 3; ++i) if (a[i] != b[i]) return false;
    return true;
}
bool equal_light(const A2FORefitLight& a, const A2FORefitLight& b) {
    return a.kind == b.kind && equal3(a.position, b.position) &&
           equal3(a.direction, b.direction) && equal3(a.colour, b.colour) &&
           a.intensity == b.intensity && a.start == b.start && a.range == b.range;
}
bool equal_output(const PreparedMaterialColours2& a, const PreparedMaterialColours2& b) {
    if (a.presentation.flags != b.presentation.flags ||
        a.presentation.alpha != b.presentation.alpha ||
        !equal3(a.presentation.team, b.presentation.team) ||
        !equal3(a.presentation.ambient, b.presentation.ambient) ||
        a.presentation.self_illum != b.presentation.self_illum ||
        a.native_ambient != b.native_ambient || a.native_diffuse != b.native_diffuse ||
        a.specular != b.specular || a.native_specular != b.native_specular || a.native_power != b.native_power ||
        a.lights.size() != b.lights.size()) return false;
    for (std::size_t i = 0; i < a.lights.size(); ++i)
        if (!equal_light(a.lights[i], b.lights[i])) return false;
    return true;
}
PreparedMaterialColours2 sentinel() {
    PreparedMaterialColours2 result;
    result.presentation.flags = 99;
    result.presentation.alpha = .37f;
    result.presentation.ambient[0] = 19;
    result.native_ambient = {17, 18, 19};
    result.native_diffuse = {23, 29, 31};
    A2FORefitLight light{};
    light.kind = A2FO_REFIT_POINT;
    light.colour[0] = 73;
    result.lights.push_back(light);
    return result;
}
MaterialColours2 material() {
    return {1, true, {7, 8, 9}, {2, 3, 4}};
}
Presentation2 presentation() {
    Presentation2 result;
    result.flags = A2FO_REFIT_TEAM;
    result.alpha = .5f;
    result.team[0] = .25f; result.team[1] = .5f; result.team[2] = .75f;
    result.ambient[0] = 1; result.ambient[1] = 2; result.ambient[2] = 3;
    return result;
}
std::vector<A2FORefitLight> lights() {
    A2FORefitLight directional{};
    directional.kind = A2FO_REFIT_DIRECTIONAL;
    directional.position[0] = 10; directional.position[1] = 11; directional.position[2] = 12;
    directional.direction[0] = -1; directional.direction[1] = -.5f; directional.direction[2] = .25f;
    directional.colour[0] = 4; directional.colour[1] = 6; directional.colour[2] = 8;
    directional.intensity = 2; directional.start = 3; directional.range = 5;
    A2FORefitLight point = directional;
    point.kind = A2FO_REFIT_POINT;
    point.position[0] = 20; point.intensity = 7; point.start = 13; point.range = 17;
    return {directional, point};
}
void rejects_unchanged(const MaterialColours2& m, const Presentation2& p,
                       const std::vector<A2FORefitLight>& source, const char* label) {
    auto output = sentinel();
    const auto before = output;
    std::string error;
    const bool result = prepare_material_colours2(m, p, source, output, error);
    expect(!result, label);
    expect(!error.empty(), "rejection supplies error");
    expect(equal_output(output, before), "rejection leaves output unchanged");
}
}

int main() {
    const auto m = material();
    const auto p = presentation();
    auto source = lights();
    const auto original = source;
    PreparedMaterialColours2 output = sentinel();
    std::string error = "stale";
    expect(prepare_material_colours2(m, p, source, output, error), "coloured team Lambert admitted");
    expect(error.empty(), "success clears error");
    expect(output.presentation.flags == 0 && output.presentation.alpha == .5f &&
           output.presentation.self_illum == 0, "TEAM flag cleared and alpha retained");
    expect(output.native_ambient == std::array<float, 3>{0, 0, 0} &&
           output.native_diffuse == std::array<float, 3>{.25f, .5f, .75f},
           "team colour replaces diffuse and neutralizes native ambient");
    expect(equal3(output.presentation.ambient, p.ambient), "team Lambert uses scene ambient");
    expect(output.lights.size() == source.size(), "Lambert copies light count");
    if (output.lights.size() == source.size())
        for (std::size_t i = 0; i < source.size(); ++i) {
            auto want = source[i];
            for (unsigned channel = 0; channel < 3; ++channel)
                want.colour[channel] *= p.team[channel];
            expect(equal_light(output.lights[i], want), "light fields preserved except RGB product");
        }
    expect(equal_light(source[0], original[0]) && equal_light(source[1], original[1]),
           "source lights unchanged");
    source[0].position[0] = 999;
    expect(output.lights[0].position[0] == 10, "output owns light copy");

    auto black = p;
    for (float& value : black.team) value = 0;
    expect(prepare_material_colours2(m, black, original, output, error), "black team admitted");
    expect(output.native_ambient == std::array<float, 3>{0, 0, 0} &&
           output.native_diffuse == std::array<float, 3>{0, 0, 0} &&
           equal3(output.presentation.ambient, black.ambient) &&
           output.lights[0].colour[0] == 0 && output.lights[0].colour[1] == 0 &&
           output.lights[0].colour[2] == 0, "black team has no diffuse or native ambient glow");

    auto no_team_material = m;
    no_team_material.team = false;
    expect(prepare_material_colours2(no_team_material, p, original, output, error),
           "nonteam authored material admitted");
    expect(output.native_ambient == m.ambient && output.native_diffuse == m.diffuse &&
           output.presentation.ambient[0] == 8 && output.presentation.ambient[1] == 10 &&
           output.presentation.ambient[2] == 12 && output.lights[0].colour[0] == 8 &&
           output.lights[0].colour[1] == 18 && output.lights[0].colour[2] == 32,
           "nonteam material uses authored ambient and diffuse");
    auto disabled = p;
    disabled.flags = 0;
    expect(prepare_material_colours2(m, disabled, original, output, error),
           "team-disabled presentation admitted");
    expect(output.native_ambient == m.ambient && output.native_diffuse == m.diffuse,
           "team material without TEAM flag uses authored colours");

    auto constant = m;
    constant.model = 0;
    auto high_scene = p;
    high_scene.ambient[0] = 100;
    expect(prepare_material_colours2(constant, high_scene, original, output, error),
           "Constant model admitted");
    expect(output.lights.empty() && output.presentation.ambient[0] == p.team[0] &&
           output.presentation.ambient[1] == p.team[1] &&
           output.presentation.ambient[2] == p.team[2],
           "Constant ignores scene ambient and lights");

    auto hdr = no_team_material;
    hdr.ambient = {10, 20, 30}; hdr.diffuse = {100, 200, 300};
    expect(prepare_material_colours2(hdr, disabled, original, output, error),
           "finite HDR values admitted");
    expect(output.presentation.ambient[0] == 11 && output.lights[0].colour[0] == 400 &&
           output.lights[0].colour[1] == 1200 && output.lights[0].colour[2] == 2400,
           "HDR sums and products are not clamped");

    auto bad_material = m;
    bad_material.model = 3;
    rejects_unchanged(bad_material, p, original, "unknown model 3 refused");
    bad_material = m; bad_material.model = 99;
    rejects_unchanged(bad_material, p, original, "unknown material refused");
    auto bad_presentation = p;
    bad_presentation.flags = A2FO_REFIT_UNLIT;
    rejects_unchanged(m, bad_presentation, original, "UNLIT refused");
    bad_presentation = p; bad_presentation.flags |= 0x80;
    rejects_unchanged(m, bad_presentation, original, "unknown flag refused");
    bad_presentation = p; bad_presentation.alpha = std::numeric_limits<float>::quiet_NaN();
    rejects_unchanged(m, bad_presentation, original, "NaN alpha refused");
    bad_presentation = p; bad_presentation.alpha = -1;
    rejects_unchanged(m, bad_presentation, original, "negative alpha refused");
    bad_presentation = p; bad_presentation.alpha = 1.1f;
    rejects_unchanged(m, bad_presentation, original, "alpha above one refused");
    bad_presentation = p; bad_presentation.self_illum = 1;
    rejects_unchanged(m, bad_presentation, original, "emissive pass retained by caller");
    bad_presentation = p; bad_presentation.team[0] = std::numeric_limits<float>::quiet_NaN();
    rejects_unchanged(m, bad_presentation, original, "NaN team refused");
    bad_presentation = p; bad_presentation.ambient[0] = -1;
    rejects_unchanged(m, bad_presentation, original, "negative scene ambient refused");
    bad_material = m; bad_material.diffuse[0] = -1;
    rejects_unchanged(bad_material, p, original, "negative material diffuse refused");
    bad_material = m; bad_material.ambient[0] = std::numeric_limits<float>::infinity();
    rejects_unchanged(bad_material, p, original, "nonfinite material ambient refused");
    auto bad_lights = original;
    bad_lights[0].colour[0] = -1;
    rejects_unchanged(m, p, bad_lights, "negative light RGB refused");
    bad_lights = original; bad_lights[0].intensity = std::numeric_limits<float>::quiet_NaN();
    rejects_unchanged(m, p, bad_lights, "NaN light intensity refused");
    bad_lights = original; bad_lights[0].intensity = -1;
    rejects_unchanged(m, p, bad_lights, "negative light intensity refused");
    bad_material = no_team_material;
    bad_material.ambient[0] = std::numeric_limits<float>::max();
    rejects_unchanged(bad_material, disabled, original, "ambient sum overflow refused");
    bad_material = no_team_material;
    bad_material.diffuse[0] = std::numeric_limits<float>::max();
    rejects_unchanged(bad_material, disabled, original, "light RGB product overflow refused");

    // Phong (model 2): Lambert colours plus the engine's specular/power rules.
    {
        auto phong = m; phong.model = 2; phong.specular = {.1f, .2f, .3f}; phong.power = 6;
        auto out = sentinel(); std::string e = "stale";
        expect(prepare_material_colours2(phong, p, source, out, e), "team Phong admitted");
        expect(out.specular && out.native_power == 6, "Phong carries material power");
        expect(out.native_specular == std::array<float, 3>{.25f, .5f, .75f}, "active team Phong specular is the team colour");
        expect(out.native_diffuse == std::array<float, 3>{.25f, .5f, .75f} && out.lights.size() == source.size(),
               "Phong diffuse and lights follow Lambert");
        auto off = p; off.flags = 0;
        out = sentinel();
        expect(prepare_material_colours2(phong, off, source, out, e), "non-team Phong admitted");
        expect(out.native_specular == std::array<float, 3>{.1f, .2f, .3f}, "inactive team Phong uses material specular");
        auto lambert = m; out = sentinel();
        expect(prepare_material_colours2(lambert, p, source, out, e) && !out.specular && out.native_power == 0,
               "Lambert carries no specular");
        auto bad = phong; bad.power = -1; rejects_unchanged(bad, p, original, "negative Phong power refused");
        bad = phong; bad.power = 2000; rejects_unchanged(bad, p, original, "Phong power above 1024 refused");
        bad = phong; bad.power = std::numeric_limits<float>::quiet_NaN(); rejects_unchanged(bad, p, original, "nonfinite Phong power refused");
        bad = phong; bad.specular[1] = -1; rejects_unchanged(bad, p, original, "negative Phong specular refused");
    }
    if (failures) std::cerr << failures << " material colour checks failed\n";
    else std::cout << "material colour preparation checks passed\n";
    return failures ? 1 : 0;
}
