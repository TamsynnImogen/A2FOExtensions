// Full production-source fixture. The runner selects frozen or current source.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#ifndef MODULE_SOURCE
#error MODULE_SOURCE is required
#endif
#include MODULE_SOURCE

namespace {
using namespace a2fo::shell_display;

std::uint32_t bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

unsigned short control_word() {
    unsigned short value = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(value));
    return value;
}

unsigned short status_word() {
    unsigned short value = 0;
    __asm__ __volatile__("fnstsw %0" : "=m"(value));
    return value;
}

void set_control_word(unsigned short value) {
    __asm__ __volatile__("fldcw %0" : : "m"(value));
}

void clear_status() {
    __asm__ __volatile__("fnclex");
}

void check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL:bink:%s\n", label);
        std::exit(2);
    }
}

struct Probe {
    unsigned short before_cw;
    unsigned short after_cw;
    unsigned short before_sw;
    unsigned short after_sw;
};

constexpr unsigned short kObservedStatus = 0x3f | 0x0200 | 0x3800;

Probe finish_probe(unsigned short requested, unsigned short before_cw,
                   unsigned short before_sw) {
    const Probe probe{before_cw, control_word(), before_sw, status_word()};
    check(probe.before_cw == requested, "requested-control-word");
    check(probe.after_cw == requested, "control-word");
    check((probe.after_sw & 0x3800) == (probe.before_sw & 0x3800), "x87-top");
    return probe;
}

void print_probe(const Probe& probe) {
    std::printf(":cw=%04x,%04x:sw=%04x,%04x\n",
                probe.before_cw, probe.after_cw,
                probe.before_sw & kObservedStatus,
                probe.after_sw & kObservedStatus);
}

struct ViewportCase {
    const char* label;
    ArmadaFloatRect rectangle;
    bool expected;
};

struct FitCase {
    const char* label;
    ArmadaFloatRect source;
    ArmadaFloatRect target;
};

struct DimensionCase {
    const char* label;
    unsigned source_width, source_height, target_width, target_height;
};
} // namespace

int main() {
    std::puts("ready:stage10-bink-native");
    std::fflush(stdout);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<ViewportCase, 12> viewports{{
        {"minimum", {0, 0, 1, 1}, true},
        {"maximum", {0, 0, 16384, 16384}, true},
        {"fractional", {-1.25f, 2.5f, 1.5f, 8191.5f}, true},
        {"zero-width", {0, 0, 0, 200}, false},
        {"zero-height", {0, 0, 320, 0}, false},
        {"negative-width", {0, 0, -1, 200}, false},
        {"over-width", {0, 0, 16385, 200}, false},
        {"over-height", {0, 0, 320, 16385}, false},
        {"nan-x", {nan, 0, 320, 200}, false},
        {"nan-width", {0, 0, nan, 200}, false},
        {"infinite-y", {0, infinity, 320, 200}, false},
        {"infinite-height", {0, 0, 320, infinity}, false},
    }};
    const std::array<FitCase, 5> rectangles{{
        {"same", {0, 0, 640, 480}, {10, 20, 640, 480}},
        {"wide-target", {0, 0, 400, 300}, {1.25f, -2.5f, 1920, 1080}},
        {"tall-target", {0, 0, 1920, 1080}, {-5, 7, 640, 900}},
        {"fractional", {0, 0, 319.5f, 199.25f}, {0.5f, 1.25f, 1023.75f, 777.5f}},
        {"one-pixel", {0, 0, 1, 1}, {0, 0, 1, 1}},
    }};
    const std::array<DimensionCase, 6> dimensions{{
        {"same", 640, 480, 640, 480},
        {"wide-target", 400, 300, 1920, 1080},
        {"tall-target", 1920, 1080, 640, 900},
        {"portrait", 300, 400, 1280, 720},
        {"minimum", 1, 1, 1, 1},
        {"maximum", 16384, 16384, 16384, 16384},
    }};

    const unsigned short original = control_word();
    constexpr std::array<unsigned short, 3> precisions{{0x0000, 0x0200, 0x0300}};
    constexpr std::array<unsigned short, 4> roundings{{0x0000, 0x0400, 0x0800, 0x0c00}};
    unsigned cases = 0;
    for (unsigned precision = 0; precision < precisions.size(); ++precision) {
        for (unsigned rounding = 0; rounding < roundings.size(); ++rounding) {
            const unsigned short requested = static_cast<unsigned short>(
                (original & ~0x0f00u) | precisions[precision] | roundings[rounding]);
            for (const auto& item : viewports) {
                clear_status();
                set_control_word(requested);
                const unsigned short before_cw = control_word();
                const unsigned short before_sw = status_word();
                const bool result = sane_viewport(item.rectangle);
                const Probe probe = finish_probe(requested, before_cw, before_sw);
                set_control_word(original);
                clear_status();
                check(result == item.expected, item.label);
                std::printf("viewport:%u:%u:%s:%u", precision, rounding,
                            item.label, static_cast<unsigned>(result));
                print_probe(probe);
                ++cases;
            }
            for (const auto& item : rectangles) {
                clear_status();
                set_control_word(requested);
                const unsigned short before_cw = control_word();
                const unsigned short before_sw = status_word();
                const ArmadaFloatRect result = fit_rectangle(item.source, item.target);
                const Probe probe = finish_probe(requested, before_cw, before_sw);
                set_control_word(original);
                clear_status();
                check(std::isfinite(result.x) && std::isfinite(result.y) &&
                      result.width >= 1 && result.height >= 1 &&
                      result.width <= item.target.width &&
                      result.height <= item.target.height, item.label);
                std::printf("rectangle:%u:%u:%s:%08x,%08x,%08x,%08x",
                            precision, rounding, item.label,
                            bits(result.x), bits(result.y),
                            bits(result.width), bits(result.height));
                print_probe(probe);
                ++cases;
            }
            for (const auto& item : dimensions) {
                unsigned width = 0, height = 0;
                clear_status();
                set_control_word(requested);
                const unsigned short before_cw = control_word();
                const unsigned short before_sw = status_word();
                fit_dimensions(item.source_width, item.source_height,
                               item.target_width, item.target_height, width, height);
                const Probe probe = finish_probe(requested, before_cw, before_sw);
                set_control_word(original);
                clear_status();
                check(width >= 1 && height >= 1 &&
                      width <= item.target_width && height <= item.target_height,
                      item.label);
                std::printf("dimensions:%u:%u:%s:%u,%u",
                            precision, rounding, item.label, width, height);
                print_probe(probe);
                ++cases;
            }
        }
    }
    set_control_word(original);
    clear_status();
    std::printf("completed:stage10-bink-native:%u\n", cases);
    return 0;
}
