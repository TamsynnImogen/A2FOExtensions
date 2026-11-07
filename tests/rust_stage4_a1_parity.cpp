// Stage-four A1 policy transcript. Compile this file once against the frozen
// C++ implementation and once against the current adapter/Rust implementation.
#include "a1_bzn_policy.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
std::uint32_t bits(float f) { std::uint32_t v; std::memcpy(&v, &f, 4); return v; }
float from_bits(std::uint32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
std::uint32_t random_word(std::uint32_t& seed) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}
void put32(std::vector<std::uint8_t>& v, std::uint32_t n) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>(n >> (i * 8)));
}
void field(std::vector<std::uint8_t>& v, std::uint8_t kind, const std::vector<std::uint8_t>& value) {
    put32(v, kind); put32(v, static_cast<std::uint32_t>(value.size()));
    v.insert(v.end(), value.begin(), value.end());
}
void float_field(std::vector<std::uint8_t>& v, float f) {
    const auto n = bits(f);
    field(v, 5, {static_cast<std::uint8_t>(n), static_cast<std::uint8_t>(n >> 8),
                 static_cast<std::uint8_t>(n >> 16), static_cast<std::uint8_t>(n >> 24)});
}
std::vector<std::uint8_t> front(int version = 2053) {
    std::string s = "version [1] =\n" + std::to_string(version) + "\nsaveGameDesc = " +
        std::string(256, 'A') + "\nbinarySave [1] =\ntrue\n";
    return {s.begin(), s.end()};
}
std::vector<std::uint8_t> full_header() {
    auto v = front();
    field(v, 2, {'m', 'a', 'p', '.', 'b', 'z', 'n', 0});
    field(v, 1, {1}); field(v, 2, std::vector<std::uint8_t>(100));
    for (float f : {-100.0f, 100.0f, -50.0f, 50.0f, -200.0f, 200.0f}) float_field(v, f);
    return v;
}
void header_case(const char* label, const std::vector<std::uint8_t>& data) {
    a1compat::A1BznHeader h;
    h.version = 700; h.metadata_offset = 901;
    std::memset(h.mission_name, 0x5a, sizeof(h.mission_name));
    h.has_map_bounds = true;
    for (int i = 0; i < 3; ++i) {
        h.minimum_extent[i] = 11.0f + i; h.map_size[i] = 22.0f + i;
    }
    const bool result = a1compat::parse_a1_bzn_header(data.data(), data.size(), &h);
    const bool null_result = a1compat::parse_a1_bzn_header(data.data(), data.size(), nullptr);
    std::printf("header %s %u %u %u %zu %u", label, unsigned(result), unsigned(null_result),
                h.version, h.metadata_offset, unsigned(h.has_map_bounds));
    for (unsigned char c : h.mission_name) std::printf(" %02x", unsigned(c));
    for (float f : h.minimum_extent) std::printf(" %08x", bits(f));
    for (float f : h.map_size) std::printf(" %08x", bits(f));
    std::putchar('\n');
}
void bounds_case(const char* label, const std::array<float, 3>& min,
                 const std::array<float, 3>& size, bool has,
                 std::uint32_t x, std::uint32_t y) {
    a1compat::A1BznHeader h;
    h.has_map_bounds = has;
    for (int i = 0; i < 3; ++i) { h.minimum_extent[i] = min[i]; h.map_size[i] = size[i]; }
    float minimum[3] = {37, 38, 39}, extent[3] = {41, 42, 43};
    const bool bounds = a1compat::a2_compatible_map_bounds(h, minimum, extent);
    float world[3] = {71, 72, 73};
    const bool position = a1compat::a1_mdf_world_position(h, {x, y}, world);
    std::printf("geometry %s %u %u", label, unsigned(bounds), unsigned(position));
    for (float f : minimum) std::printf(" %08x", bits(f));
    for (float f : extent) std::printf(" %08x", bits(f));
    for (float f : world) std::printf(" %08x", bits(f));
    std::putchar('\n');
}
void mdf_case(const char* label, const std::string& input) {
    a1compat::A1MdfData m;
    m.start_location_count = 77;
    for (unsigned i = 0; i < 8; ++i) m.start_locations[i] = {999u + i, 888u};
    const bool result = a1compat::parse_a1_mdf(input.data(), input.size(), &m);
    std::printf("mdf %s %u %zu", label, unsigned(result), m.start_location_count);
    for (const auto& location : m.start_locations)
        std::printf(" %u %u", location.x, location.y);
    std::putchar('\n');
}
std::vector<std::uint8_t> mission(const char* name) {
    std::vector<std::uint8_t> v; put32(v, 2); put32(v, 40);
    v.insert(v.end(), name, name + std::strlen(name)); v.resize(48);
    return v;
}
void object(std::vector<std::uint8_t>& v) {
    field(v, 7, {'s', 'h', 'i', 'p', 0}); field(v, 4, {0, 0, 0, 0});
    field(v, 9, std::vector<std::uint8_t>(12)); field(v, 4, {0, 0, 0, 0});
    field(v, 2, std::vector<std::uint8_t>(40));
}
void tail_case(const char* label, const std::vector<std::uint8_t>& data) {
    a1compat::A1BznObjectTailLayout t{701, 702, 703};
    const bool result = a1compat::locate_a1_bzn_object_tail(data.data(), data.size(), &t);
    std::printf("tail %s %u %zu %zu %u", label, unsigned(result),
                t.first_object_offset, t.mission_offset, t.object_count);
    const auto* end = data.data() + data.size();
    for (std::size_t at = 0; at < data.size(); ++at)
        std::printf("%c", a1compat::serialized_a1_mission_record_at(data.data() + at, end) ? '1' : '0');
    std::putchar('\n');
}
#if defined(__i386__) || defined(__x86_64__)
unsigned short control_word() { unsigned short cw; __asm__ __volatile__("fnstcw %0" : "=m"(cw)); return cw; }
void set_control_word(unsigned short cw) { __asm__ __volatile__("fldcw %0" : : "m"(cw)); }
#endif
void numeric_cases(unsigned mode) {
    const char* fixed[] = {"ordinary", "vertical", "denormal", "max", "disabled",
                           "nan_min_x", "nan_min_y", "nan_min_z", "nan_size_x",
                           "nan_size_y", "nan_size_z", "posinf_min", "neginf_min",
                           "posinf_size", "neginf_size", "signed_zero"};
    auto emit = [mode](const char* name, std::array<float, 3> min,
                       std::array<float, 3> size, bool has = true) {
        char label[80]; std::snprintf(label, sizeof(label), "p%u_%s", mode, name);
        bounds_case(label, min, size, has, 38, 79);
    };
    const float nan = from_bits(0x7fc12345u), inf = std::numeric_limits<float>::infinity();
    const float den = std::numeric_limits<float>::denorm_min();
    const float max = std::numeric_limits<float>::max();
    emit(fixed[0], {-100, -50, -200}, {200, 100, 400});
    emit(fixed[1], {0, -1500, 0}, {1, 3000, 1});
    emit(fixed[2], {den, -1250, -1000000}, {max, den, 333333.31f});
    emit(fixed[3], {0, max, 0}, {1, max, 1});
    emit(fixed[4], {0, 0, 0}, {1, 1, 1}, false);
    emit(fixed[5], {nan, 0, 0}, {1, 1, 1});
    emit(fixed[6], {0, nan, 0}, {1, 1, 1});
    emit(fixed[7], {0, 0, nan}, {1, 1, 1});
    emit(fixed[8], {0, 0, 0}, {nan, 1, 1});
    emit(fixed[9], {0, 0, 0}, {1, nan, 1});
    emit(fixed[10], {0, 0, 0}, {1, 1, nan});
    emit(fixed[11], {0, inf, 0}, {1, 1, 1});
    emit(fixed[12], {0, -inf, 0}, {1, 1, 1});
    emit(fixed[13], {0, 0, 0}, {1, inf, 1});
    emit(fixed[14], {0, 0, 0}, {1, -inf, 1});
    emit(fixed[15], {-0.0f, -0.0f, -0.0f}, {den, -0.0f, den});
    const std::array<float, 6> exceptional = {nan, inf, -inf, 0.0f, -0.0f, den};
    const char* names[] = {"nan", "posinf", "neginf", "poszero", "negzero", "denorm"};
    for (unsigned which = 0; which < exceptional.size(); ++which) {
        for (unsigned axis = 0; axis < 3; ++axis) {
            for (unsigned target = 0; target < 2; ++target) {
                std::array<float, 3> minima{2.0f, -10.0f, 4.0f};
                std::array<float, 3> sizes{1.0f, 20.0f, 3.0f};
                (target == 0 ? minima : sizes)[axis] = exceptional[which];
                char label[80];
                std::snprintf(label, sizeof(label), "p%u_%s_%s_%u", mode, names[which],
                              target == 0 ? "min" : "size", axis);
                bounds_case(label, minima, sizes, true, 38, 79);
            }
        }
    }
    for (unsigned axis = 0; axis < 3; ++axis) {
        auto minima = std::array<float, 3>{2.0f, -10.0f, 4.0f};
        auto sizes = std::array<float, 3>{1.0f, 20.0f, 3.0f};
        minima[axis] = max;
        char label[80]; std::snprintf(label, sizeof(label), "p%u_max_min_%u", mode, axis);
        bounds_case(label, minima, sizes, true, 38, 79);
        minima = {2.0f, -10.0f, 4.0f}; sizes[axis] = max;
        std::snprintf(label, sizeof(label), "p%u_max_size_%u", mode, axis);
        bounds_case(label, minima, sizes, true, 38, 79);
    }
    bounds_case("invalid_118", {0, 0, 0}, {1, 1, 1}, true, 118, 0);
    bounds_case("corner_0", {0, 0, 0}, {1, 1, 1}, true, 0, 0);
    bounds_case("corner_117", {0, 0, 0}, {1, 1, 1}, true, 117, 117);
    std::uint32_t seed = 0x51a31d77u;
    for (unsigned i = 0; i < 256; ++i) {
        const float mx = float(int(random_word(seed) % 200001u) - 100000) / 64.0f;
        const float my = float(int(random_word(seed) % 200001u) - 100000) / 64.0f;
        const float mz = float(int(random_word(seed) % 200001u) - 100000) / 64.0f;
        const float sx = float(random_word(seed) % 100000u + 1u) / 128.0f;
        const float sy = float(random_word(seed) % 100000u + 1u) / 128.0f;
        const float sz = float(random_word(seed) % 100000u + 1u) / 128.0f;
        const auto x = random_word(seed) % 118u, y = random_word(seed) % 118u;
        char label[80]; std::snprintf(label, sizeof(label), "p%u_random_%03u", mode, i);
        bounds_case(label, {mx, my, mz}, {sx, sy, sz}, true, x, y);
    }
}
}  // namespace

int main() {
    auto h = front(); header_case("front", h);
    h.back() = 'x'; header_case("bad_boolean", h);
    for (int version : {2049, 2050, 2051, 2052, 2053, 2054}) {
        char label[40]; std::snprintf(label, sizeof(label), "version_%d", version);
        header_case(label, front(version));
    }
    h = full_header(); header_case("full", h);
    for (std::size_t cut : std::array<std::size_t, 8>{0, 1, 8, 32, 128, 256, 289, h.size() - 1}) {
        if (cut > h.size()) continue;
        char label[40]; std::snprintf(label, sizeof(label), "trunc_%zu", cut);
        header_case(label, {h.begin(), h.begin() + cut});
    }
    for (std::size_t at : std::array<std::size_t, 5>{0, 15, 33, 100, h.size() - 12}) {
        auto changed = h; changed[at] ^= 0x80u;
        char label[40]; std::snprintf(label, sizeof(label), "mutate_%zu", at);
        header_case(label, changed);
    }
    const std::array<std::string, 13> mdfs = {{
        "StartLocations=2\nStart1=0 117\nStart2=117 0\n",
        " # ignored\r\nStartLocations = 0\r\n", "StartLocations=1\nStart1=118 0",
        "StartLocations=1\nStart1=1 2\nStart1=3 4", "StartLocations=9",
        "StartLocations=2\nStart1=1 2", "StartLocations=x",
        "StartLocations=1\nStart1=4294967296 0", "StartLocations=1\nStart1=1 2\nOther=ascii",
        std::string("StartLocations=1\nStart1=1 2\nBad=") + char(0xff),
        "StartLocations=8\nStart1=0 0\nStart2=1 1\nStart3=2 2\nStart4=3 3\nStart5=4 4\nStart6=5 5\nStart7=6 6\nStart8=117 117",
        "StartLocations=1\rStart1=1 2\r", "StartLocations=1\nStart0=1 2" }};
    for (unsigned i = 0; i < mdfs.size(); ++i) {
        char label[40]; std::snprintf(label, sizeof(label), "case_%02u", i);
        mdf_case(label, mdfs[i]);
    }
    const std::string valid_mdf = mdfs[0];
    for (std::size_t cut = 0; cut < valid_mdf.size(); cut += 3) {
        char label[40]; std::snprintf(label, sizeof(label), "trunc_%zu", cut);
        mdf_case(label, valid_mdf.substr(0, cut));
    }
    std::vector<std::uint8_t> tail(7, 0xee); object(tail);
    auto marker = mission("Inst4XMission"); tail.insert(tail.end(), marker.begin(), marker.end());
    tail_case("valid", tail);
    auto duplicate = tail; duplicate.insert(duplicate.end(), marker.begin(), marker.end());
    tail_case("duplicate", duplicate);
    tail_case("only_mission", mission("EmptyMission"));
    for (std::size_t cut : std::array<std::size_t, 6>{0, 1, 7, 12, 48, tail.size() - 1}) {
        char label[40]; std::snprintf(label, sizeof(label), "trunc_%zu", cut);
        tail_case(label, {tail.begin(), tail.begin() + cut});
    }
    for (std::size_t at : std::array<std::size_t, 7>{7, 11, 16, 24, 36, tail.size() - 48, tail.size() - 44}) {
        auto changed = tail; changed[at] ^= 0x80u;
        char label[40]; std::snprintf(label, sizeof(label), "mutate_%zu", at);
        tail_case(label, changed);
    }
#if defined(__i386__) || defined(__x86_64__)
    const auto saved = control_word();
#if defined(_WIN32)
    for (unsigned mode : {0u, 2u, 3u}) {
        set_control_word(static_cast<unsigned short>((saved & ~0x0300u) | (mode << 8)));
        numeric_cases(mode);
    }
#else
    numeric_cases(3);
#endif
    set_control_word(saved);
#else
    numeric_cases(3);
#endif
    return 0;
}
