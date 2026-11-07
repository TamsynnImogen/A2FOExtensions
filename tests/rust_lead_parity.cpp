#include "d3d8_tokens.hpp"
#include "fallback_policy.hpp"
#include "object_editor_state.hpp"
#include "shield_visibility.hpp"
#include "vs11.hpp"
#ifdef _WIN32
#include "fvf.hpp"
#endif
#include "fixtures/renderer_vs11_capture.inc"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::uint32_t rng = 0xb62135a9u;
std::uint32_t next() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
void bytes(std::string_view s) {
    std::printf(" %zu:", s.size());
    for (unsigned char c : s)
        std::printf("%02x", c);
}
float number(std::uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
void shaders() {
    using namespace a2fo::renderer::shaders;
    std::vector<std::vector<std::uint32_t>> corpus = {
        {},
        {0xffffffffu},
        {0, 0xffffffffu},
        {0xfffe0101u, 1, 0xc00f0000u, 0x90e40000u, 0xffffu},
        {0xffff0104u, 0x42, 0x80000000u, 0xb0000000u, 0xfffd, 0xffff},
        {0x88000000u, 0xffffffffu},
        {0xa2000000u, 0, 0, 0xffffffffu}};
    corpus.emplace_back(program_1, program_1 + std::size(program_1));
    corpus.emplace_back(program_2, program_2 + std::size(program_2));
    corpus.emplace_back(program_4, program_4 + std::size(program_4));
    bytes(vs11_output_interface());
    std::puts("");
    for (unsigned i = 0; i < 2200; ++i) {
        auto w = corpus[i % corpus.size()];
        if (i >= corpus.size()) {
            if (i % 4 == 0 && !w.empty())
                w.resize(next() % (w.size() + 1));
            else if (i % 4 == 1 && !w.empty())
                w[next() % w.size()] ^= 1u << (next() % 32);
            else if (i % 4 == 2)
                w.push_back(next());
            else if (!w.empty())
                w[next() % w.size()] = next();
        }
        std::uint32_t dummy = 0;
        const auto *data = w.empty() ? &dummy : w.data();
        auto d = parse_d3d8_vertex_declaration(data, w.size()),
             s = parse_d3d8_shader_bytecode(data, w.size());
        std::printf("shader/%u %u %zu %u %zu", i, static_cast<unsigned>(d.status), d.consumed_words,
                    static_cast<unsigned>(s.status), s.consumed_words);
        Vs11Translation out;
        out.hlsl = "stale";
        out.input_mask = 0xffff;
        out.output_masks.fill(15);
        out.instruction_count = 999;
        std::string error = "old";
        const bool ok = translate_vs11(data, w.size(), out, error);
        std::printf(" %u %u %zu", ok, out.input_mask, out.instruction_count);
        for (auto x : out.output_masks)
            std::printf(" %u", x);
        bytes(out.hlsl);
        bytes(error);
        std::puts("");
    }
    const auto d = parse_d3d8_vertex_declaration(nullptr, 0),
               s = parse_d3d8_shader_bytecode(nullptr, 0);
    std::printf("shader-null %u %zu %u %zu\n", static_cast<unsigned>(d.status), d.consumed_words,
                static_cast<unsigned>(s.status), s.consumed_words);
}
void fallback() {
    std::vector<std::string> names = {"",
                                      "fed_scout",
                                      "FED",
                                      std::string("a\0b", 3),
                                      std::string("\xff\xc0", 2),
                                      std::string(253, 'a'),
                                      std::string(254, 'b'),
                                      std::string(255, 'c'),
                                      std::string(256, 'd')};
    for (unsigned i = 0; i < names.size(); ++i)
        for (unsigned j = 0; j < names.size(); ++j) {
            auto r = a1fallbacks::sprite_fallback_names(names[i], names[j]);
            std::printf("fallback/%u/%u", i, j);
            bytes(r.build_button);
            bytes(r.faction_icon);
            std::puts("");
        }
}
void shields() {
    using namespace a2fo::shields;
    for (unsigned i = 0; i < 1000; ++i) {
        const auto now = next(), last = next(), interval = next();
        const auto effect = static_cast<std::int32_t>(next());
        const float current = number(next());
        std::printf("shields/%u %u %u\n", i,
                    static_cast<unsigned>(choose_effect_action((i & 1) != 0, current, effect)),
                    global_scan_due(now, last, (i & 2) != 0, interval));
    }
    for (auto bits : {0u, 0x80000000u, 1u, 0x7fc00001u, 0x7f800000u, 0xff800000u, 0x3f800000u}) {
        for (int effect : {-1, 0, 1})
            std::printf("shield-edge/%u/%d %u\n", bits, effect,
                        static_cast<unsigned>(choose_effect_action(true, number(bits), effect)));
    }
}
std::string encoded(const a2fo::object_editor::SavedState &state) {
    const auto *b = reinterpret_cast<const unsigned char *>(&state);
    std::string out = "a2fo_objectEditor = ";
    const char *h = "0123456789ABCDEF";
    for (std::size_t i = 0; i < sizeof(state); ++i) {
        out += h[b[i] >> 4];
        out += h[b[i] & 15];
    }
    return out + "\r\nother\n";
}
void editor() {
    using namespace a2fo::object_editor;
    for (unsigned i = 0; i < 500; ++i) {
        SavedState state;
        state.flags = i % 8;
        state.captain[0] = 'c';
        state.registry[0] = 'r';
        for (unsigned j = 0; j < 4; ++j) {
            state.current[j] = number(next());
            state.maximum[j] = number(next());
        }
        if (i % 3 == 0) {
            state.current = {1, 2, 0, 4};
            state.maximum = {5, 6, 0, 8};
        }
        if (i % 11 == 0)
            state.version = 2;
        if (i % 13 == 0)
            state.size = 1075;
        if (i % 17 == 0)
            state.captain.fill('x');
        std::printf("editor/%u %u", i, valid_state(state));
        for (unsigned kind = 0; kind < 3; ++kind) {
            std::string input;
            if (kind == 2)
                input = encoded(state);
            else {
                if (kind == 1) {
                    std::uint32_t type = 0x100, count = sizeof(state);
                    input.append(reinterpret_cast<const char *>(&type), 4);
                    input.append(reinterpret_cast<const char *>(&count), 4);
                }
                input.append(reinterpret_cast<const char *>(&state), sizeof(state));
                input += "trailing";
            }
            if (i % 7 == 0 && !input.empty())
                input.resize(next() % input.size());
            if (i % 19 == 0 && !input.empty())
                input[next() % input.size()] ^= 0x40;
            SavedState output;
            output.flags = 0x777;
            std::size_t used = 999;
            const auto status = decode_prefix(input, kind != 2, kind == 1, &output, &used);
            std::printf(" %u %zu", static_cast<unsigned>(status), used);
            bytes(std::string_view(reinterpret_cast<const char *>(&output), sizeof(output)));
        }
        std::puts("");
    }
    for (const auto *text :
         {"", "1", "-0", "0x1p2", "1e-999", "nan", "inf", "1.5 ", "1.5\v", "-2.0", "1x"}) {
        float output = 42;
        const bool ok = parse_shield_number(text, &output);
        std::uint32_t bits;
        std::memcpy(&bits, &output, 4);
        std::printf("editor-number %u %08x", ok, bits);
        bytes(text);
        std::puts("");
    }
}
#ifdef _WIN32
void fvf() {
    using namespace a2fo::renderer::compat;
    for (unsigned i = 0; i < 2400; ++i) {
        std::uint32_t code = i < 2048 ? i : next();
        FvfLayout l;
        l.stride = 999;
        auto hr = decode_fvf(code, l);
        std::printf("fvf/%u %08x %08lx %u %u %u %u %u %u %u %u %u %u %u", i, code,
                    static_cast<unsigned long>(hr), l.stride, l.transformed, l.position_components,
                    l.normal, l.diffuse, l.specular, l.texture_count, l.position_offset,
                    l.normal_offset, l.diffuse_offset, l.specular_offset);
        for (unsigned j = 0; j < 8; ++j)
            std::printf(" %u %u", l.texture_offsets[j], l.texture_components[j]);
        std::puts("");
    }
    for (unsigned kind = 0; kind < 9; ++kind)
        for (std::uint32_t count : {0u, 1u, 10u, 0xffffffffu, 0xfffffffeu}) {
            UINT result = 999;
            auto hr = primitive_vertex_count(kind, count, result);
            std::printf("primitive/%u/%u %08lx %u\n", kind, count, static_cast<unsigned long>(hr),
                        result);
        }
    for (unsigned i = 0; i < 240; ++i) {
        const unsigned count = i % 17, vertices = i % 23;
        std::vector<std::uint32_t> source(count + 2);
        for (auto &x : source)
            x = next() % 25;
        std::vector<std::uint32_t> out{999, 998};
        const void *input = i % 3 == 0 ? nullptr : source.data();
        auto hr = expand_triangle_fan(count, input, 102, vertices, out);
        std::printf("fan/%u %08lx %zu", i, static_cast<unsigned long>(hr), out.size());
        for (auto x : out)
            std::printf(" %u", x);
        std::puts("");
    }
}
#endif
} // namespace
int main() {
    shaders();
    fallback();
    shields();
    editor();
#ifdef _WIN32
    fvf();
#endif
}
