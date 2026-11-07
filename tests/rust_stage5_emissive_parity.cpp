// Link once against frozen C++ and once against Rust-backed C++.
#include "../core/nebula_emissive.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>
namespace nb = a2fo::nebula;
static std::uint32_t seed = 0x5e1ab73du;
static std::uint32_t next() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
static void bits(float value) {
    std::uint32_t raw;
    std::memcpy(&raw, &value, 4);
    std::printf(" %08x", raw);
}
static void bits(double value) {
    std::uint64_t raw;
    std::memcpy(&raw, &value, 8);
    std::printf(" %016llx", static_cast<unsigned long long>(raw));
}
static void text(const std::string &value) {
    std::printf(" %zu", value.size());
    for (unsigned char c : value)
        std::printf(" %02x", c);
}
static void pixels(const std::vector<std::uint32_t> &value) {
    std::printf(" %zu", value.size());
    for (auto pixel : value)
        std::printf(" %08x", pixel);
}
static void run() {
    const double dnan = std::numeric_limits<double>::quiet_NaN();
    const double dinf = std::numeric_limits<double>::infinity();
    const float fnan = std::numeric_limits<float>::quiet_NaN();
    const float finf = std::numeric_limits<float>::infinity();
    const double hp[] = {
        -dinf, -1.0, 0.0, 0.00001, 9.99989, 9.9999, std::nextafter(9.9999, dinf),
        10.0,  dinf, dnan};
    const float disable[] = {-finf, -1.0f, -0.0f, 0.0f, 0.0001f, finf, fnan};
    for (int op = 0; op < 2; ++op)
        for (int forced = 0; forced < 2; ++forced)
            for (int maximum : {-1, 0, 10, 1000})
                for (double current : hp)
                    for (float timer : disable) {
                        std::printf("sub %d %d %d", op, forced, maximum);
                        bits(current);
                        bits(timer);
                        std::printf(
                            " %d\n",
                            static_cast<int>(nb::classify_subsystem_light(
                                op != 0, forced != 0, maximum, current, timer)));
                    }
    const float speeds[] = {-finf,
                            -1.0f,
                            -0.0f,
                            0.0f,
                            std::nextafter(0.01f, 0.0f),
                            0.01f,
                            std::nextafter(0.01f, finf),
                            1.0f,
                            1.1025f,
                            std::nextafter(1.1025f, finf),
                            100.0f,
                            finf,
                            fnan};
    for (std::uint32_t warp : {0u, 1u, 2u, 3u, 4u, 0xffffffffu})
        for (float velocity : speeds)
            for (float impulse : {0.0f, 1.0f, 10.0f, finf, fnan}) {
                std::printf("motion %08x", warp);
                bits(velocity);
                bits(impulse);
                const auto state =
                    nb::classify_craft_motion_light(warp, velocity, impulse);
                std::printf(" %u", static_cast<unsigned>(state));
                const auto intensity = nb::emissive_intensity_percent(state);
                for (auto percent : intensity)
                    std::printf(" %08x", percent);
                std::putchar('\n');
            }
    const std::vector<std::string> keys = {"",
                                           " ",
                                           "  / ",
                                           "fbattle",
                                           "Fbattle.tga",
                                           "Textures/RGB/Fbattle.dds",
                                           "C:\\Ships\\Tûrret.ODF  ",
                                           ".hidden",
                                           "foo.",
                                           "a/b/c.tar.gz",
                                           "...",
                                           "\\",
                                           " /x/. ",
                                           std::string("a\0b.TGA", 7),
                                           std::string({char(0xff), 'A', '/', char(0xfe), 'B', '.', 'D', 'D', 'S'})};
    for (const auto &value : keys) {
        std::printf("key");
        text(value);
        text(nb::normalize_texture_key(value));
        std::putchar('\n');
    }
    for (int i = 0; i < 256; ++i) {
        std::array<std::uint32_t, 6> sources{};
        std::array<std::uint32_t, 6> intensities{};
        for (int j = 0; j < 6; ++j) {
            sources[j] = i % 7 == 0 ? 0xff000000u : next();
            intensities[j] = i % 11 == 0  ? 0xffffffffu
                             : i % 5 == 0 ? 0u
                                          : next() % 401u;
        }
        const auto mask = static_cast<std::uint8_t>(i % 8 == 0 ? 0x3fu : next());
        std::printf("merge %d %02x", i, mask);
        for (auto pixel : sources)
            std::printf(" %08x", pixel);
        for (auto percent : intensities)
            std::printf(" %08x", percent);
        std::printf(" %08x %08x", nb::combine_emissive_pixel(sources, mask),
                    nb::combine_emissive_pixel(sources, mask, intensities));
        std::printf(" %08x\n",
                    nb::merge_emissive_pixel(sources[0], sources[1], intensities[0]));
    }
    for (int i = 0; i < 256; ++i) {
        std::size_t width = static_cast<std::size_t>(1 + next() % 8u);
        std::size_t height = static_cast<std::size_t>(1 + next() % 7u);
        const std::size_t length = (i % 17 == 0) ? width * height - 1 : width * height;
        if (i % 23 == 0)
            width = 0;
        if (i % 29 == 0)
            height = 0;
        const std::size_t radius = i % 13 == 0 ? std::numeric_limits<std::size_t>::max()
                                               : static_cast<std::size_t>(i % 12);
        const std::uint32_t strength = i % 19 == 0  ? 0xffffffffu
                                       : i % 7 == 0 ? 0u
                                                    : static_cast<std::uint32_t>(i * 7);
        std::vector<std::uint32_t> image(length);
        for (auto &pixel : image)
            pixel = i % 5 == 0 ? 0xff000000u : next();
        std::printf("bloom %d %zu %zu %zu %08x", i, width, height, radius, strength);
        pixels(image);
        const bool result =
            nb::add_emissive_bloom(image, width, height, radius, strength);
        std::printf(" %d", result ? 1 : 0);
        pixels(image);
        std::putchar('\n');
    }
}
int main() {
#if defined(_WIN32) && defined(__i386__)
    unsigned short saved;
    __asm__ __volatile__("fnstcw %0" : "=m"(saved));
    for (unsigned short precision :
         {static_cast<unsigned short>(0x0000), static_cast<unsigned short>(0x0200),
          static_cast<unsigned short>(0x0300)}) {
        unsigned short cw = (saved & ~0x0300) | precision;
        __asm__ __volatile__("fldcw %0" ::"m"(cw));
        std::printf("precision %04x\n", precision);
        run();
    }
    __asm__ __volatile__("fldcw %0" ::"m"(saved));
#else
    run();
#endif
}
