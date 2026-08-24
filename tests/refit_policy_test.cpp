#include "docking_transform.hpp"
#include "refit_policy.hpp"

#include <cassert>
#include <cmath>

namespace {

bool close(float left, float right, float tolerance = 0.0001f) {
    return std::fabs(left - right) <= tolerance;
}

}  // namespace

int main() {
    using a2fo::refit::OdfFields;
    using a2fo::refit::normalize_odf_value;
    using a2fo::refit::parse_class_policy;

    assert(normalize_odf_value("  \"fgalaxy\" \r\n") == "fgalaxy");
    assert(normalize_odf_value("'build'") == "build");

    const auto source = parse_class_policy(OdfFields{
        {"refitItem0", "\"fgalaxy\""},
        {"REFITITEM1", " fneghvar \t"},
        {"refitItem2", "FGALAXY"},
    });
    assert(source.is_refit_source());
    assert(source.refit_items.size() == 2);
    assert(source.refit_items[0] == "fgalaxy");
    assert(source.refit_items[1] == "fneghvar");

    const auto yard = parse_class_policy(OdfFields{
        {"classLabel", "\"shipyard\""},
        {"buildHardpoint", "\"build\""},
        {"refitHardpoint", "\"BUILD\""},
    });
    assert(yard.is_supported_yard());

    const auto mismatched = parse_class_policy(OdfFields{
        {"classLabel", "shipyard"},
        {"buildHardpoint", "build"},
        {"refitHardpoint", "refit"},
    });
    assert(!mismatched.is_supported_yard());

    const auto wrong_class = parse_class_policy(OdfFields{
        {"classLabel", "research"},
        {"buildHardpoint", "build"},
        {"refitHardpoint", "build"},
    });
    assert(!wrong_class.is_supported_yard());

    a2fo::refit::DockingTransform origin{};
    origin.values[0] = 1.0f;
    origin.values[4] = 1.0f;
    origin.values[8] = 1.0f;
    a2fo::refit::DockingTransform destination{};
    destination.values[1] = 1.0f;
    destination.values[3] = -1.0f;
    destination.values[8] = 1.0f;
    destination.values[9] = 100.0f;
    destination.values[10] = 200.0f;
    destination.values[11] = 300.0f;

    const auto approach = a2fo::refit::docking_approach_transform(
        destination, 125.0f);
    for (int index = 0; index < 9; ++index) {
        assert(close(approach.values[index], destination.values[index]));
    }
    assert(close(approach.values[9], 100.0f));
    assert(close(approach.values[10], 200.0f));
    assert(close(approach.values[11], 175.0f));

    auto diagonal_hardpoint = destination;
    diagonal_hardpoint.values[6] = 3.0f;
    diagonal_hardpoint.values[7] = 4.0f;
    diagonal_hardpoint.values[8] = 0.0f;
    const auto diagonal_approach =
        a2fo::refit::docking_approach_transform(
            diagonal_hardpoint, 50.0f);
    assert(close(diagonal_approach.values[9], 70.0f));
    assert(close(diagonal_approach.values[10], 160.0f));
    assert(close(diagonal_approach.values[11], 300.0f));

    auto invalid_hardpoint = destination;
    invalid_hardpoint.values[6] = 0.0f;
    invalid_hardpoint.values[7] = 0.0f;
    invalid_hardpoint.values[8] = 0.0f;
    const auto fallback_approach =
        a2fo::refit::docking_approach_transform(
            invalid_hardpoint, 40.0f);
    assert(close(fallback_approach.values[9], 100.0f));
    assert(close(fallback_approach.values[10], 200.0f));
    assert(close(fallback_approach.values[11], 260.0f));

    const auto start = a2fo::refit::interpolate_docking_transform(
        origin, destination, 0.0f);
    const auto middle = a2fo::refit::interpolate_docking_transform(
        origin, destination, 0.5f);
    const auto finish = a2fo::refit::interpolate_docking_transform(
        origin, destination, 1.0f);
    for (int index = 0; index < 12; ++index) {
        assert(close(start.values[index], origin.values[index]));
        assert(close(finish.values[index], destination.values[index]));
    }
    assert(close(middle.values[9], 50.0f));
    assert(close(middle.values[10], 100.0f));
    assert(close(middle.values[11], 150.0f));
    const float right_length = std::sqrt(
        middle.values[0] * middle.values[0] +
        middle.values[1] * middle.values[1] +
        middle.values[2] * middle.values[2]);
    const float up_length = std::sqrt(
        middle.values[3] * middle.values[3] +
        middle.values[4] * middle.values[4] +
        middle.values[5] * middle.values[5]);
    const float right_up_dot =
        middle.values[0] * middle.values[3] +
        middle.values[1] * middle.values[4] +
        middle.values[2] * middle.values[5];
    assert(close(right_length, 1.0f));
    assert(close(up_length, 1.0f));
    assert(close(right_up_dot, 0.0f));
    return 0;
}
