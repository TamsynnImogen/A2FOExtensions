#include "../modules/A2FOM3D/m3d_conversion.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <array>
#include <string>
#include <vector>

namespace {

struct W {
    std::vector<std::uint8_t> b;
    void u8(std::uint8_t v) { b.push_back(v); }
    void i8(std::int8_t v) { b.push_back(static_cast<std::uint8_t>(v)); }
    void u16(std::uint16_t v) {
        b.push_back(static_cast<std::uint8_t>(v));
        b.push_back(static_cast<std::uint8_t>(v >> 8u));
    }
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void f32(float v) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }
    void id(const std::string& s) {
        u16(static_cast<std::uint16_t>(s.size()));
        b.insert(b.end(), s.begin(), s.end());
    }
};

void matrix(W& w) {
    const float m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    for (float v : m) w.f32(v);
}

std::vector<std::uint8_t> make_m3d_196() {
    W w;
    const char h[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), h, h + 10);
    w.f32(1.96f);

    w.u16(1);
    w.id("mat");
    const float colours[9] = {0.1f,0.1f,0.1f, 1.f,1.f,1.f, 0.5f,0.5f,0.5f};
    for (float v : colours) w.f32(v);
    w.f32(32.f);
    w.u8(2);
    w.u8(0);

    w.u16(1);
    w.id("mesh");
    w.u32(1);
    w.u16(1);
    w.u32(0);
    matrix(w);

    w.u32(0);
    w.u16(0);
    w.u16(3);
    w.u16(3);
    w.u16(1);
    const float verts[9] = {0,0,0, 1,0,0, 0,1,0};
    for (float v : verts) w.f32(v);
    for (int i = 0; i < 3; ++i) { w.f32(0); w.f32(0); w.f32(1); }
    const float uvs[6] = {0,0, 1,0, 0,1};
    for (float v : uvs) w.f32(v);

    w.u16(1);
    w.id("mat");
    w.u32(2);
    w.id("Textures\\shiptex.dds");
    w.u32(0);
    w.id("shipbump.TGA");
    w.u32(0x200);
    w.f32(1.0f);
    w.id("default");
    for (std::uint16_t i = 0; i < 3; ++i) { w.u16(i); w.u16(i); }
    w.i8(1);
    w.u16(0);

    w.u16(0);
    w.u32(9);
    w.u16(0);
    return w.b;
}


struct R {
    const std::vector<std::uint8_t>& b;
    std::size_t p = 0;
    std::uint8_t u8() { assert(p < b.size()); return b[p++]; }
    std::uint16_t u16() {
        assert(p + 2 <= b.size());
        const auto v = static_cast<std::uint16_t>(b[p] | (static_cast<std::uint16_t>(b[p + 1]) << 8u));
        p += 2; return v;
    }
    std::uint32_t u32() {
        assert(p + 4 <= b.size());
        std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(b[p + i]) << (8u * i);
        p += 4; return v;
    }
    float f32() { const auto bits = u32(); float v = 0; std::memcpy(&v, &bits, 4); return v; }
    std::string id() {
        const auto n = u16(); assert(p + n <= b.size());
        std::string v(reinterpret_cast<const char*>(b.data() + p), n); p += n; return v;
    }
};

void validate_sod_193(const std::vector<std::uint8_t>& b) {
    R r{b};
    assert(b.size() >= 14);
    assert(std::memcmp(b.data(), "Storm3D_SW", 10) == 0); r.p = 10;
    assert(std::fabs(r.f32() - 1.93f) < 0.001f);
    assert(r.u16() == 1);
    assert(r.id() == "mat");
    for (int i = 0; i < 9; ++i) (void)r.f32();
    (void)r.f32(); assert(r.u8() == 2); assert(r.u8() == 0);
    assert(r.u16() == 1);
    assert(r.u16() == 1); assert(r.id() == "mesh"); assert(r.id().empty());
    for (int i = 0; i < 12; ++i) (void)r.f32();
    assert(r.id() == "default");
    assert(r.u32() == 0);              // diffuse-only diagnostic: DOT3 disabled
    assert(r.u32() == 1);              // diffuse only; bump retained out-of-band
    assert(r.id() == "shiptex");
    assert(r.u32() == 0);              // diffuse texture load flags
    assert(r.id().empty());            // >=1.92 Borg/assimilation texture; empty derives texture0 + _b
    assert(r.u16() == 0);              // >=1.91 extra field
    assert(r.u16() == 3); assert(r.u16() == 3); assert(r.u16() == 1);
    for (int i = 0; i < 9; ++i) (void)r.f32();
    for (int i = 0; i < 6; ++i) (void)r.f32();
    assert(r.u16() == 1); assert(r.id() == "mat");
    const int winding[3] = {0, 2, 1};
    for (int i = 0; i < 3; ++i) {
        assert(r.u16() == winding[i]);
        assert(r.u16() == winding[i]);
    }
    assert(r.u8() == 1); assert(r.u16() == 0);
    assert(r.u16() == 0); assert(r.u16() == 0);
    assert(r.p == b.size());
}


std::vector<std::uint8_t> make_m3d_196_two_roots_with_zero_id() {
    W w;
    const char h[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), h, h + 10);
    w.f32(1.96f);
    w.u16(0); // materials
    w.u16(2); // nodes

    w.id("zero_root"); w.u32(0); w.u16(0); w.u32(0); matrix(w);
    w.id("other_root"); w.u32(7); w.u16(0); w.u32(0); matrix(w);

    w.u16(0); // animations
    w.u32(0); // >=1.96 database value
    w.u16(0); // references
    return w.b;
}

void validate_multi_root_normalization(const std::vector<std::uint8_t>& b) {
    R r{b};
    assert(std::memcmp(b.data(), "Storm3D_SW", 10) == 0); r.p = 10;
    assert(std::fabs(r.f32() - 1.93f) < 0.001f);
    assert(r.u16() == 0); // materials
    assert(r.u16() == 2); // ID 0 is a real root; ID 7 legitimately parents to it

    assert(r.u16() == 0); assert(r.id() == "zero_root");
    assert(r.id().empty());
    for (int i = 0; i < 12; ++i) (void)r.f32();

    assert(r.u16() == 0); assert(r.id() == "other_root");
    assert(r.id() == "zero_root");
    for (int i = 0; i < 12; ++i) (void)r.f32();

    assert(r.u16() == 0); // animations
    assert(r.u16() == 0); // texture refs
    assert(r.p == b.size());
}


std::vector<std::uint8_t> make_m3d_196_child_before_parent() {
    W w;
    const char h[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), h, h + 10);
    w.f32(1.96f);
    w.u16(0); // materials
    w.u16(3); // nodes, intentionally reverse hierarchy order

    w.id("grandchild"); w.u32(3); w.u16(0); w.u32(2); matrix(w);
    w.id("child");      w.u32(2); w.u16(0); w.u32(1); matrix(w);
    w.id("root");       w.u32(1); w.u16(0); w.u32(0); matrix(w);

    w.u16(0); // animations
    w.u32(0); // >=1.96 database value
    w.u16(0); // references
    return w.b;
}

void validate_parent_before_child_order(const std::vector<std::uint8_t>& b) {
    R r{b};
    assert(std::memcmp(b.data(), "Storm3D_SW", 10) == 0); r.p = 10;
    assert(std::fabs(r.f32() - 1.93f) < 0.001f);
    assert(r.u16() == 0); // materials
    assert(r.u16() == 3); // nodes

    assert(r.u16() == 0); assert(r.id() == "root"); assert(r.id().empty());
    for (int i = 0; i < 12; ++i) (void)r.f32();

    assert(r.u16() == 0); assert(r.id() == "child"); assert(r.id() == "root");
    for (int i = 0; i < 12; ++i) (void)r.f32();

    assert(r.u16() == 0); assert(r.id() == "grandchild"); assert(r.id() == "child");
    for (int i = 0; i < 12; ++i) (void)r.f32();

    assert(r.u16() == 0); // animations
    assert(r.u16() == 0); // texture refs
    assert(r.p == b.size());
}



std::vector<std::uint8_t> make_m3d_196_duplicate_names() {
    W w;
    const char h[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), h, h + 10);
    w.f32(1.96f);
    w.u16(0); // materials
    w.u16(5); // nodes, children before parents with duplicate "lights"

    w.id("red");    w.u32(12); w.u16(0); w.u32(10); matrix(w);
    w.id("green");  w.u32(22); w.u16(0); w.u32(20); matrix(w);
    w.id("lights"); w.u32(10); w.u16(0); w.u32(1);  matrix(w);
    w.id("lights"); w.u32(20); w.u16(0); w.u32(1);  matrix(w);
    w.id("root");   w.u32(1);  w.u16(0); w.u32(0);  matrix(w);

    w.u16(0); w.u32(0); w.u16(0);
    return w.b;
}

void validate_duplicate_name_hierarchy(const std::vector<std::uint8_t>& b) {
    R r{b};
    assert(std::memcmp(b.data(), "Storm3D_SW", 10) == 0); r.p = 10;
    assert(std::fabs(r.f32() - 1.93f) < 0.001f);
    assert(r.u16() == 0);
    assert(r.u16() == 5);

    assert(r.u16() == 0); assert(r.id() == "root"); assert(r.id().empty());
    for (int i=0;i<12;++i) (void)r.f32();

    assert(r.u16() == 0);
    const std::string lights10 = r.id();
    assert(lights10 == "lights_m3d_10"); assert(r.id() == "root");
    for (int i=0;i<12;++i) (void)r.f32();

    assert(r.u16() == 0);
    const std::string lights20 = r.id();
    assert(lights20 == "lights_m3d_20"); assert(r.id() == "root");
    for (int i=0;i<12;++i) (void)r.f32();

    assert(r.u16() == 0); assert(r.id() == "red"); assert(r.id() == lights10);
    for (int i=0;i<12;++i) (void)r.f32();

    assert(r.u16() == 0); assert(r.id() == "green"); assert(r.id() == lights20);
    for (int i=0;i<12;++i) (void)r.f32();
    assert(r.u16() == 0); assert(r.u16() == 0); assert(r.p == b.size());
}

std::vector<std::uint8_t> make_m3d_196_multigroup_sparse(const std::string& mesh_name = "hull") {
    W w;
    const char h[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), h, h + 10);
    w.f32(1.96f);
    w.u16(1); w.id("mat");
    for (int i = 0; i < 9; ++i) w.f32(i < 6 ? 1.0f : 0.0f);
    w.f32(8.0f); w.u8(1); w.u8(0);

    w.u16(2); // mesh precedes its parent in the source stream
    w.id(mesh_name); w.u32(1); w.u16(1); w.u32(7); matrix(w);
    w.u32(0); w.u16(0);
    w.u16(6); w.u16(6); w.u16(2);
    const float verts[18] = {
        0,0,0, 1,0,0, 0,1,0,
        10,0,0, 11,0,0, 10,1,0};
    for (float v : verts) w.f32(v);
    for (int i = 0; i < 6; ++i) { w.f32(0); w.f32(0); w.f32(1); }
    const float uvs[12] = {0,0,1,0,0,1, 0,0,1,0,0,1};
    for (float v : uvs) w.f32(v);

    // group 1 uses only source vertices/UVs 0,1,2
    w.u16(1); w.id("mat"); w.u32(1); w.id("a.dds"); w.u32(0); w.id("default");
    for (std::uint16_t i : {0u,1u,2u}) { w.u16(i); w.u16(i); }
    // group 2 uses only source vertices/UVs 3,4,5
    w.u16(1); w.id("mat"); w.u32(1); w.id("b.dds"); w.u32(0); w.id("default");
    for (std::uint16_t i : {3u,4u,5u}) { w.u16(i); w.u16(i); }

    w.i8(1); w.u16(0);
    w.id("root"); w.u32(7); w.u16(0); w.u32(0); matrix(w);
    w.u16(0); w.u32(0); w.u16(0);
    return w.b;
}

void skip_sod_mesh_payload_and_assert_compact(R& r, const std::string& expected_name,
                                              const std::string& expected_texture) {
    assert(r.u16() == 1);
    assert(r.id() == expected_name);
    assert(r.id() == "root");
    for (int i = 0; i < 12; ++i) (void)r.f32();
    assert(r.id() == "default");
    assert(r.u32() == 0);
    assert(r.u32() == 1);
    assert(r.id() == expected_texture);
    assert(r.u32() == 0);
    assert(r.id().empty());
    assert(r.u16() == 0);
    assert(r.u16() == 3); // compacted vertices, not original 6
    assert(r.u16() == 3); // compacted UVs, not original 6
    assert(r.u16() == 1);
    for (int i = 0; i < 9; ++i) (void)r.f32();
    for (int i = 0; i < 6; ++i) (void)r.f32();
    assert(r.u16() == 1); assert(r.id() == "mat");
    const int winding[3] = {0,2,1};
    for (int i = 0; i < 3; ++i) {
        assert(r.u16() == winding[i]);
        assert(r.u16() == winding[i]);
    }
    assert(r.u8() == 1); assert(r.u16() == 0);
}

void validate_multigroup_compaction(const std::vector<std::uint8_t>& b,
                                    const std::string& mesh_name = "hull") {
    R r{b};
    assert(std::memcmp(b.data(), "Storm3D_SW", 10) == 0); r.p = 10;
    assert(std::fabs(r.f32() - 1.93f) < 0.001f);
    assert(r.u16() == 1); assert(r.id() == "mat");
    for (int i = 0; i < 9; ++i) (void)r.f32();
    (void)r.f32(); (void)r.u8(); (void)r.u8();
    assert(r.u16() == 3);
    assert(r.u16() == 0); assert(r.id() == "root"); assert(r.id().empty());
    for (int i = 0; i < 12; ++i) (void)r.f32();
    skip_sod_mesh_payload_and_assert_compact(r, mesh_name, "a");
    skip_sod_mesh_payload_and_assert_compact(r, mesh_name + "_a2fo_g2", "b");
    assert(r.u16() == 0); assert(r.u16() == 0);
    assert(r.p == b.size());
}

void validate_multigroup_runtime_bindings(const a2fo::m3d::ConversionStats& stats,
                                          const std::string& mesh_name) {
    assert(stats.meshes == 1);
    assert(stats.groups == 2);
    assert(stats.triangles == 2);
    assert(stats.output_nodes == 3);
    assert(stats.source_roots == 1);
    assert(stats.reordered_nodes != 0);
    assert(stats.hierarchy_bindings.size() == 3);
    assert(stats.hierarchy_bindings[0].node_name == "root");
    assert(stats.hierarchy_bindings[0].parent_name.empty());
    assert(stats.hierarchy_bindings[1].node_name == mesh_name);
    assert(stats.hierarchy_bindings[1].parent_name == "root");
    assert(stats.hierarchy_bindings[2].node_name == mesh_name + "_a2fo_g2");
    assert(stats.hierarchy_bindings[2].parent_name == "root");
    assert(stats.texture_bindings.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& binding = stats.texture_bindings[i];
        assert(binding.mesh_name == (i == 0 ? mesh_name : mesh_name + "_a2fo_g2"));
        assert(binding.diffuse_texture == (i == 0 ? "a.dds" : "b.dds"));
        assert(binding.has_local_bounds);
        assert(binding.local_bounds_min == (std::array<float, 3>{i == 0 ? 0.f : 10.f, 0.f, 0.f}));
        assert(binding.local_bounds_max == (std::array<float, 3>{i == 0 ? 1.f : 11.f, 1.f, 0.f}));
    }
}


std::vector<std::uint8_t> make_m3d_196_lod_node() {
    W w;
    const char h[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), h, h + 10);
    w.f32(1.96f);
    w.u16(0); // materials
    w.u16(1); // nodes
    w.id("legacy_lod"); w.u32(1); w.u16(11); w.u32(0); matrix(w);
    w.u16(0); // animations
    w.u32(0); // >=1.96 database value
    w.u16(0); // references
    return w.b;
}

void validate_lod_neutralization(const std::vector<std::uint8_t>& b) {
    R r{b};
    assert(std::memcmp(b.data(), "Storm3D_SW", 10) == 0); r.p = 10;
    assert(std::fabs(r.f32() - 1.93f) < 0.001f);
    assert(r.u16() == 0); // materials
    assert(r.u16() == 1); // nodes
    assert(r.u16() == 0); // Legacy type-11 LOD is emitted as null transform
    assert(r.id() == "legacy_lod");
    assert(r.id().empty());
    for (int i = 0; i < 12; ++i) (void)r.f32();
    assert(r.u16() == 0); // animations
    assert(r.u16() == 0); // texture refs
    assert(r.p == b.size());
}

void validate_damage_emitter_adaptation(int variant = 0) {
    // 0: ordinary static ship; 1: missing selector; 2: duplicate selector;
    // 3: singular selector; 4: animation; 5: child on emitter; 6: reference.
    struct N {
        std::string name;
        std::uint32_t id, parent;
        std::uint16_t type = 0;
        std::array<float, 12> m{1,0,0,0,1,0,0,0,1,0,0,0};
    };
    std::vector<N> nodes{
        {"root", 1, 0}, {"geometry", 2, 1}, {"damage", 3, 1},
        {variant == 1 ? "unused" : "engines", 4, 3}, {"target", 5, 3},
        {"l_engine", 6, 2}, {"r_engine", 7, 2}, {"weapons", 8, 2},
        {"break", 9, 6}, {"break", 10, 7}, {"break", 11, 8},
        {"vent_l", 12, 9, 12}, {"vent_r", 13, 10, 12}, {"vent_w", 14, 11, 12},
        {"always_on", 15, 6, 12}, {"already_damage", 16, 4, 12},
        {"unknown_part", 17, 2}, {"break", 18, 17}, {"unknown_vent", 19, 18, 12}};
    nodes[0].m[9] = 100;
    nodes[1].m[9] = 10;
    nodes[2].m[9] = -10;
    for (std::size_t i : {3u, 4u}) { nodes[i].m[0] = 2; nodes[i].m[10] = 5; }
    for (std::size_t i : {5u, 6u, 7u}) nodes[i].m = {0,2,0,-3,0,0,0,0,1,0,20,0};
    for (std::size_t i : {8u, 9u, 10u}) nodes[i].m[9] = 4;
    for (std::size_t i : {11u, 12u, 13u}) nodes[i].m[9] = 2;
    if (variant == 2) nodes.push_back({"engines", 20, 3});
    if (variant == 3) nodes[3].m[0] = 0;
    if (variant == 5) nodes.push_back({"attached", 20, 12});
    W w;
    const char magic[10] = {'M','a','d','3','D','_','S','W',0,0};
    w.b.insert(w.b.end(), magic, magic + 10);
    w.f32(1.96f); w.u16(0); w.u16(static_cast<std::uint16_t>(nodes.size()));
    // Child-first input with duplicate break names, as shipped by Legacy.
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        w.id(it->name); w.u32(it->id); w.u16(it->type); w.u32(it->parent);
        for (float v : it->m) w.f32(v);
        if (it->type == 12) w.id("plasmalrg");
    }
    w.u16(variant == 4 ? 1 : 0);
    if (variant == 4) {
        w.id("movement"); w.u32(1); w.u16(1); w.f32(1); w.u8(0); w.u8(0); matrix(w);
    }
    w.u32(0); w.u16(variant == 6 ? 1 : 0);
    if (variant == 6) { w.u8(0); w.id("l_engine"); w.id("movement"); w.u32(0); }
    std::vector<std::uint8_t> out;
    a2fo::m3d::ConversionStats stats;
    std::string error;
    assert(a2fo::m3d::convert_bytes_to_sod(w.b, out, stats, error));
    const bool engine_adapted = variant == 0 || variant == 5;
    const bool weapon_adapted = variant != 4 && variant != 6;
    const std::size_t expected_count = (engine_adapted ? (variant == 5 ? 1 : 2) : 0) +
                                       (weapon_adapted ? 1 : 0);
    assert(stats.damage_emitters_adapted == expected_count);
    assert(stats.output_nodes == nodes.size());
    R r{out}; r.p = 14;
    assert(r.u16() == 0); assert(r.u16() == nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto type = r.u16();
        const auto name = r.id(); const auto parent = r.id();
        std::array<float, 12> m{};
        for (auto& value : m) value = r.f32();
        if (type == 12) assert(r.id() == "plasmalrg");
        const bool mapped = (name == "vent_l" && engine_adapted && variant != 5) ||
                            (name == "vent_r" && engine_adapted) ||
                            (name == "vent_w" && weapon_adapted);
        if (mapped) {
            assert(parent == (name == "vent_w" ? "target" : "engines"));
            // Independently derived: source world position=(110,32,0),
            // selector world position=(90,5,0), selector X scale=2.
            const std::array<float, 12> expected{0,2,0,-1.5f,0,0,0,0,1,10,27,0};
            for (std::size_t j = 0; j < m.size(); ++j) assert(std::fabs(m[j] - expected[j]) < 1e-5f);
        } else if (name == "vent_l" || name == "vent_r" || name == "vent_w") {
            assert(parent == "break_m3d_" + std::string(name == "vent_l" ? "9" : name == "vent_r" ? "10" : "11"));
            assert(m[9] == 2);
        }
        if (name == "always_on") assert(parent == "l_engine");
        if (name == "unknown_vent") assert(parent == "break_m3d_18");
        if (name == "already_damage") assert(parent == (variant == 2 ? "engines_m3d_4" : variant == 1 ? "unused" : "engines"));
        if (name == "engines" || name == "target") assert(parent == "damage");
        if (name == "break_m3d_9") assert(parent == "l_engine");
        if (name == "break_m3d_10") assert(parent == "r_engine");
        if (name == "break_m3d_11") assert(parent == "weapons");
    }
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& b, std::size_t p) {
    return static_cast<std::uint16_t>(b[p] | (static_cast<std::uint16_t>(b[p + 1]) << 8u));
}

}  // namespace

int main(int argc, char** argv) {
    for (int variant = 0; variant <= 6; ++variant) validate_damage_emitter_adaptation(variant);
    const auto input = make_m3d_196();
    std::vector<std::uint8_t> output;
    a2fo::m3d::ConversionStats stats;
    std::string error;
    assert(a2fo::m3d::convert_bytes_to_sod(input, output, stats, error));
    assert(output.size() > 40);
    assert(std::memcmp(output.data(), "Storm3D_SW", 10) == 0);
    float version = 0.0f;
    std::memcpy(&version, output.data() + 10, 4);
    assert(std::fabs(version - 1.93f) < 0.001f);
    assert(stats.materials == 1);
    assert(stats.meshes == 1);
    assert(stats.groups == 1);
    assert(stats.triangles == 1);
    assert(stats.output_nodes == 1);
    assert(read_u16(output, 14) == 1);
    validate_sod_193(output);
    assert(stats.source_roots == 1);
    assert(!stats.synthesized_root);

    const auto multi_root_input = make_m3d_196_two_roots_with_zero_id();
    std::vector<std::uint8_t> multi_root_output;
    a2fo::m3d::ConversionStats multi_root_stats;
    assert(a2fo::m3d::convert_bytes_to_sod(
        multi_root_input, multi_root_output, multi_root_stats, error));
    assert(multi_root_stats.source_roots == 1);
    assert(!multi_root_stats.synthesized_root);
    assert(multi_root_stats.output_nodes == 2);
    validate_multi_root_normalization(multi_root_output);

    const auto reverse_input = make_m3d_196_child_before_parent();
    std::vector<std::uint8_t> reverse_output;
    a2fo::m3d::ConversionStats reverse_stats;
    assert(a2fo::m3d::convert_bytes_to_sod(
        reverse_input, reverse_output, reverse_stats, error));
    assert(reverse_stats.source_roots == 1);
    assert(!reverse_stats.synthesized_root);
    assert(reverse_stats.reordered_nodes != 0);
    validate_parent_before_child_order(reverse_output);

    const auto duplicate_input = make_m3d_196_duplicate_names();
    std::vector<std::uint8_t> duplicate_output;
    a2fo::m3d::ConversionStats duplicate_stats;
    assert(a2fo::m3d::convert_bytes_to_sod(duplicate_input, duplicate_output, duplicate_stats, error));
    assert(duplicate_stats.source_roots == 1);
    validate_duplicate_name_hierarchy(duplicate_output);

    const auto multigroup_input = make_m3d_196_multigroup_sparse();
    std::vector<std::uint8_t> multigroup_output;
    a2fo::m3d::ConversionStats multigroup_stats;
    assert(a2fo::m3d::convert_bytes_to_sod(
        multigroup_input, multigroup_output, multigroup_stats, error));
    validate_multigroup_compaction(multigroup_output);
    validate_multigroup_runtime_bindings(multigroup_stats, "hull");

    for (const std::string mesh_name : {
             "in_l_engine_0", "in_pod_0", "in_r_engine_0", "in_f_akira_a",
             "In_L_EnGiNe_0"}) {
        const auto interior_input = make_m3d_196_multigroup_sparse(mesh_name);
        std::vector<std::uint8_t> interior_output;
        a2fo::m3d::ConversionStats interior_stats;
        assert(a2fo::m3d::convert_bytes_to_sod(
            interior_input, interior_output, interior_stats, error));
        validate_multigroup_compaction(interior_output, mesh_name);
        validate_multigroup_runtime_bindings(interior_stats, mesh_name);
    }


    const auto lod_input = make_m3d_196_lod_node();
    std::vector<std::uint8_t> lod_output;
    a2fo::m3d::ConversionStats lod_stats;
    assert(a2fo::m3d::convert_bytes_to_sod(
        lod_input, lod_output, lod_stats, error));
    validate_lod_neutralization(lod_output);

    std::vector<std::uint8_t> passthrough = {
        'S','t','o','r','m','3','D','_','S','W',1,2,3};
    std::vector<std::uint8_t> copied;
    a2fo::m3d::ConversionStats pass_stats;
    assert(a2fo::m3d::convert_bytes_to_sod(
        passthrough, copied, pass_stats, error));
    assert(pass_stats.legacy_sod_passthrough);
    assert(copied == passthrough);

    if (argc > 1) {
        // Optional shipped Akira fixture; never write the user's asset.
        std::ifstream file(argv[1], std::ios::binary);
        assert(file);
        const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>{file}, {});
        std::vector<std::uint8_t> akira_output;
        a2fo::m3d::ConversionStats akira;
        assert(a2fo::m3d::convert_bytes_to_sod(bytes, akira_output, akira, error));
        assert(akira.damage_emitters_adapted == 3);
        assert(akira.groups == 13 && akira.texture_bindings.size() == 13);
        assert(akira.output_nodes == 90 && akira.triangles == 5898);
        std::size_t engine_vents = 0, weapon_vents = 0;
        for (const auto& node : akira.hierarchy_bindings) {
            if (node.authored_name == "plasmalrg") {
                assert(node.parent_name == "engines" || node.parent_name == "target");
                engine_vents += node.parent_name == "engines";
                weapon_vents += node.parent_name == "target";
            }
            if (node.node_name == "engines" || node.node_name == "target") assert(node.parent_name == "damage");
        }
        assert(engine_vents == 2 && weapon_vents == 1);
        std::cout << "Akira: 3 damage vents, 90 nodes, 13 mesh groups, 5898 triangles\n";
    }
    std::cout << "m3d conversion tests passed\n";
    return 0;
}
