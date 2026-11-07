#include "build_time_text.hpp"
#if defined(A2FO_DIRECT_DECAL_BINDINGS)
#include "../rust/text_geometry_ffi.h"
#else
#include "decal_math.hpp"
#endif
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
static std::uint32_t bits(float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    return n;
}
static float value(std::uint32_t n) {
    float f;
    std::memcpy(&f, &n, 4);
    return f;
}
static void hex(const char *p, std::size_t n) {
    const char *d = "0123456789abcdef";
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char c = p[i];
        std::cout << d[c >> 4] << d[c & 15];
    }
}
struct Precision {
#if defined(__i386__) && defined(__GNUC__)
    unsigned short old{};
    explicit Precision(unsigned n) {
        __asm__ volatile("fnstcw %0" : "=m"(old));
        unsigned short next = (old & ~0x300u) | (n << 8);
        __asm__ volatile("fldcw %0" ::"m"(next));
    }
    ~Precision() { __asm__ volatile("fldcw %0" ::"m"(old)); }
#else
    explicit Precision(unsigned) {}
#endif
};
static std::uint32_t seed = 0x4c65416du;
static std::uint32_t next_word() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
int main() {
    using namespace a2fo::build_tooltips;
    const std::array<float, 20> special{{0.0f,
                                         -0.0f,
                                         1.0f,
                                         -1.0f,
                                         0.49999997f,
                                         0.5f,
                                         1.5f,
                                         1000.0f,
                                         100000000.0f,
                                         4294967040.0f,
                                         4294967296.0f,
                                         std::numeric_limits<float>::max(),
                                         std::numeric_limits<float>::lowest(),
                                         std::numeric_limits<float>::min(),
                                         std::numeric_limits<float>::denorm_min(),
                                         std::numeric_limits<float>::infinity(),
                                         -std::numeric_limits<float>::infinity(),
                                         value(0x7fc12345),
                                         value(0x7f812345),
                                         0.001f}};
#if defined(__i386__) && defined(__GNUC__)
    const unsigned modes[] = {0, 2, 3};
#else
    const unsigned modes[] = {3};
#endif
    for (unsigned mode : modes) {
        Precision precision(mode);
        for (unsigned i = 0; i < 2000; ++i) {
            float raw = i < special.size() ? special[i] : value(next_word());
            float native = i < special.size() ? special[special.size() - i - 1] : value(next_word());
            float modifier = i < special.size() ? special[i] : value(next_word());
            if (i >= special.size() && i % 3 == 0) {
                raw = std::fabs(raw);
                native = i % 2 ? raw : std::nextafter(raw, std::numeric_limits<float>::infinity());
                modifier = float((next_word() % 10000) + 1) / 1000.0f;
            }
            std::cout << "reconcile " << mode << ' ' << i << ' ' << bits(raw) << ',' << bits(native)
                      << ',' << bits(modifier) << ' '
                      << bits(reconcile_global_build_time(raw, native, modifier)) << '\n';
        }
        for (unsigned i = 0; i < special.size() + 150; ++i) {
            const float seconds = i < special.size() ? special[i] : value(next_word());
            for (unsigned size : {0u, 1u, 4u, 15u, 64u})
                for (bool compact : {false, true}) {
                    char output[64];
                    std::memset(output, 0x7e, sizeof output);
                    bool ok = format_build_time_token(i % 7 == 0 ? nullptr : "|", seconds, compact,
                                                      output, size);
                    std::cout << "time " << mode << ' ' << i << ' ' << size << ' ' << compact << ' '
                              << ok << ' ';
                    hex(output, sizeof output);
                    std::cout << '\n';
                }
        }
        for (unsigned i = 0; i < 500; ++i) {
            std::array<float,3> v{}, d{};
            for (unsigned n = 0; n < 3; ++n) {
                v[n] = (int(next_word() % 200000) - 100000) * 0.013f;
                d[n] = (int(next_word() % 100000) - 50000) * 0.017f;
            }
            if (i == 0) {
                v = {1, 2, 3};
                d = {0, 0, 0};
            }
            if (i == 1) {
                d = {90, 180, 270};
            }
            if (i == 2) {
                v = {0.0f, -0.0f, 1.0f};
                d = {-0.0f, 0.0f, 0.0f};
            }
            if (i > 2 && i < 12) {
                v[i % 3] = special[i + 4];
                d[(i + 1) % 3] = special[i];
            }
#if defined(A2FO_DIRECT_DECAL_BINDINGS)
            std::array<float,3> out{};
            a2fo_rs_geometry_rotate_xyz(v.data(),d.data(),out.data());
#else
            auto out = a2fo::decal::rotate_xyz(v, d);
#endif
            std::cout << "rotate " << mode << ' ' << i << ' ';
            for (float f : out)
                std::cout << bits(f) << ',';
            std::cout << '\n';
        }
    }
    for (const char *sep : {static_cast<const char *>(nullptr), "", "|", "\x81\xff"})
        for (const char *name : {static_cast<const char *>(nullptr), "", "Tritanium", "\x81\xff"})
            for (std::int32_t cost : {-1, 0, 1, 1234, std::numeric_limits<std::int32_t>::max()})
                for (unsigned size : {0u, 1u, 5u, 64u})
                    for (bool compact : {false, true}) {
                        char output[64];
                        std::memset(output, 0x7e, sizeof output);
                        bool ok =
                            format_resource_cost_token(sep, name, cost, compact, output, size);
                        std::cout << "resource " << bool(sep) << ' ' << bool(name) << ' ' << cost
                                  << ' ' << size << ' ' << compact << ' ' << ok << ' ';
                        hex(output, sizeof output);
                        std::cout << '\n';
                    }
    char out[32]{};
    std::cout << "null_output " << format_build_time_token("|", 1, false, nullptr, 32)
              << format_resource_cost_token("|", "x", 1, false, nullptr, 32)
              << format_build_time_token("|", 1, false, out, 0) << '\n';
}
