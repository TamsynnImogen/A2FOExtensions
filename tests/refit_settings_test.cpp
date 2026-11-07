#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <limits>

#include "../modules/refit_common/settings.hpp"

using namespace a2fo::refit;

int main() {
    assert(parse_number(" \t.25e1\r", 0.25f, 10.0f) == 2.5f);
    assert(parse_number("0.25", 0.25f, 10.0f) == 0.25f);
    assert(parse_number("10", 0.25f, 10.0f) == 10.0f);
    assert(!parse_number("", 0.0f, 10.0f));
    assert(!parse_number("  \n ", 0.0f, 10.0f));
    assert(!parse_number("1 2", 0.0f, 10.0f));
    assert(!parse_number("1garbage", 0.0f, 10.0f));
    assert(!parse_number("1,5", 0.0f, 10.0f));
    assert(!parse_number("nan", 0.0f, 10.0f));
    assert(!parse_number("inf", 0.0f, 10.0f));
    assert(!parse_number("1e999", 0.0f, 10.0f));
    assert(!parse_number("0.249", 0.25f, 10.0f));
    assert(!parse_number("10.001", 0.25f, 10.0f));
    assert(!parse_number("1", 10.0f, 0.0f));

    assert(parse_enabled(" \t0\n") == false);
    assert(parse_enabled("1") == true);
    assert(!parse_enabled("true"));
    assert(!parse_enabled("01"));
    assert(!parse_enabled("1 0"));

    assert(CameraSettings{}.enabled == false);
    assert(CameraSettings{}.pan_speed == kDefaultPanSpeed);
    assert(ViewSettings{}.enabled == false);
    assert(ViewSettings{}.minimum_distance == 0.0f);
    const auto camera = parse_camera_settings("1", "2.5");
    assert(camera && camera->enabled && camera->pan_speed == 2.5f);
    const auto disabled_camera = parse_camera_settings("0", "0.25");
    assert(disabled_camera && !disabled_camera->enabled &&
           disabled_camera->pan_speed == 0.25f);
    assert(!parse_camera_settings("0", "broken"));
    assert(!parse_camera_settings("broken", "2.5"));

    const auto view = parse_view_settings("1", "0.125");
    assert(view && view->enabled && view->minimum_distance == 0.125f);
    const auto disabled_view = parse_view_settings("0", "1000000");
    assert(disabled_view && !disabled_view->enabled &&
           disabled_view->minimum_distance == 1000000.0f);
    assert(!parse_view_settings("0", "bogus"));
    assert(!parse_view_settings("1", "1000001"));
    assert(!parse_view_settings("1", "-1"));

    const std::uint32_t zero = float_to_bits(0.0f);
    const std::uint32_t one = float_to_bits(1.0f);
    const std::uint32_t two = float_to_bits(2.0f);
    const std::uint32_t negative_zero = float_to_bits(-0.0f);
    const std::uint32_t infinity = float_to_bits(
        std::numeric_limits<float>::infinity());
    const std::uint32_t nan = float_to_bits(
        std::numeric_limits<float>::quiet_NaN());
    assert(raise_positive_finite_bits(zero, one) == one);
    assert(raise_positive_finite_bits(one, two) == two);
    assert(raise_positive_finite_bits(two, one) == two);
    assert(raise_positive_finite_bits(one, one) == one);
    assert(raise_positive_finite_bits(one, zero) == one);
    assert(raise_positive_finite_bits(one, negative_zero) == one);
    assert(raise_positive_finite_bits(one, infinity) == one);
    assert(raise_positive_finite_bits(one, nan) == one);
    assert(raise_positive_finite_bits(negative_zero, two) == negative_zero);
    assert(raise_positive_finite_bits(float_to_bits(-1.0f), two) ==
           float_to_bits(-1.0f));
    assert(raise_positive_finite_bits(infinity, two) == infinity);
    assert(raise_positive_finite_bits(nan, two) == nan);
    assert(raise_positive_finite_bits(0x00000001u, one) == one);
}
