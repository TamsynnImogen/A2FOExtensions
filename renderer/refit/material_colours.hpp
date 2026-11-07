#pragma once

#include "packet.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace a2fo::refit {

struct MaterialColours2 {
    std::uint32_t model=0; // 0 Constant, 1 Lambert, 2 Phong (per-pixel Blinn-Phong highlight)
    bool team=false;
    std::array<float, 3> ambient{};
    std::array<float, 3> diffuse{};
    std::array<float, 3> specular{}; // model 2 only
    float power=0;                    // model 2 only
};

struct PreparedMaterialColours2 {
    Presentation2 presentation{};
    std::vector<A2FORefitLight> lights;
    std::array<float, 3> native_ambient{};
    std::array<float, 3> native_diffuse{};
    // Model 2: the engine's workspace specular (+0x70) and power (+0x7c), also the
    // highlight sent to the renderer. Zero/absent for models 0 and 1.
    bool specular=false;
    std::array<float, 3> native_specular{};
    float native_power=0;
};

// Pure colour translation. Failure leaves output unchanged. Packet validation
// remains responsible for light geometry, kinds, and renderer limits.
inline bool prepare_material_colours2(const MaterialColours2& material,
    const Presentation2& presentation, const std::vector<A2FORefitLight>& lights,
    PreparedMaterialColours2& output, std::string& error) noexcept {
    const auto fail = [&error](const char* reason) noexcept {
        try { error = reason; } catch (...) { error.clear(); }
        return false;
    };
    const auto valid = [](float value) noexcept {
        return std::isfinite(value) && value >= 0;
    };
    const auto product = [](float a, float b, float& out) noexcept {
        const double result = double(a) * b;
        if (!std::isfinite(result) || result > std::numeric_limits<float>::max())
            return false;
        out = static_cast<float>(result);
        return std::isfinite(out);
    };
    const auto sum = [](float a, float b, float& out) noexcept {
        // Test the remaining float headroom before addition: max_float + 1
        // rounds back to max_float in double precision at this magnitude.
        const double maximum = std::numeric_limits<float>::max();
        if (double(b) > maximum - double(a) || double(a) > maximum - double(b))
            return false;
        const double result = double(a) + b;
        if (!std::isfinite(result) || result > std::numeric_limits<float>::max())
            return false;
        out = static_cast<float>(result);
        return std::isfinite(out);
    };
    try {
        if (material.model > 2) return fail("unsupported material colour model");
        if (material.model == 2 && (!std::isfinite(material.power) || material.power < 0 || material.power > 1024))
            return fail("invalid material specular power");
        if (presentation.flags & ~std::uint32_t(A2FO_REFIT_TEAM))
            return fail("unsupported presentation flags");
        if (!std::isfinite(presentation.alpha) || presentation.alpha < 0 ||
            presentation.alpha > 1 || presentation.self_illum != 0)
            return fail("invalid alpha or self illumination");
        for (unsigned i = 0; i < 3; ++i)
            if (!valid(presentation.team[i]) || !valid(presentation.ambient[i]) ||
                !valid(material.ambient[i]) || !valid(material.diffuse[i]) ||
                (material.model == 2 && !valid(material.specular[i])))
                return fail("invalid material or presentation colour");
        for (const auto& light : lights) {
            if (!valid(light.intensity)) return fail("invalid light intensity");
            for (float colour : light.colour)
                if (!valid(colour)) return fail("invalid light colour");
        }

        PreparedMaterialColours2 next;
        next.presentation = presentation;
        next.presentation.flags &= ~std::uint32_t(A2FO_REFIT_TEAM);
        const bool active_team = material.team && (presentation.flags & A2FO_REFIT_TEAM);
        for (unsigned i = 0; i < 3; ++i) {
            next.native_ambient[i] = active_team ? 0.f : material.ambient[i];
            next.native_diffuse[i] = active_team ? presentation.team[i] : material.diffuse[i];
            if (material.model == 0) {
                next.presentation.ambient[i] = next.native_diffuse[i];
            } else if (!sum(presentation.ambient[i], next.native_ambient[i],
                            next.presentation.ambient[i])) {
                return fail("ambient colour sum overflows float");
            }
        }
        if (material.model == 2) {
            // Material-copy 0x6481d0: an active team material takes the team colour
            // as specular too; power is always the material's.
            next.specular = true;
            next.native_power = material.power;
            for (unsigned i = 0; i < 3; ++i)
                next.native_specular[i] = active_team ? presentation.team[i] : material.specular[i];
        }
        if (material.model >= 1) {
            next.lights = lights;
            for (auto& light : next.lights)
                for (unsigned i = 0; i < 3; ++i)
                    if (!product(light.colour[i], next.native_diffuse[i], light.colour[i]))
                        return fail("diffuse light colour overflows float");
        }
        output = std::move(next);
        error.clear();
        return true;
    } catch (...) {
        return fail("material colour preparation allocation failed");
    }
}

} // namespace a2fo::refit
