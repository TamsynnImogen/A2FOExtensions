#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foStage8NebulaBytes {
    const std::uint8_t* data;
    std::size_t size;
};
using A2foStage8NebulaPathStep = std::uint32_t (*)(
    void*, std::uint32_t, const std::uint8_t*, std::size_t,
    std::uint32_t, A2foStage8NebulaBytes*);
using A2foStage8NebulaEmit = void (*)(
    void*, const std::uint8_t*, std::size_t);
using A2foStage8NebulaFold = std::uint8_t (*)(std::uint8_t);
struct A2foStage8NebulaSplit {
    std::size_t base_size;
    std::size_t suffix_index;
};
void a2fo_rs_c_nebula_resolve_texture(
    void*, const std::uint8_t*, std::size_t,
    A2foStage8NebulaPathStep, const std::uint8_t*);
void a2fo_rs_c_nebula_suffixes(
    const A2foStage8NebulaBytes*, std::size_t,
    A2foStage8NebulaEmit, void*, const std::uint8_t*);
std::size_t a2fo_rs_c_nebula_extension_end(
    const std::uint8_t*, std::size_t);
std::size_t a2fo_rs_c_nebula_material_key(
    const std::uint8_t*, std::size_t, A2foStage8NebulaFold,
    std::uint8_t*);
A2foStage8NebulaSplit a2fo_rs_c_nebula_split_name(
    const std::uint8_t*, std::size_t,
    const A2foStage8NebulaBytes*, std::size_t,
    A2foStage8NebulaFold);
using A2foStage8NebulaMapStep = std::uint32_t (*)(
    void*, std::uint32_t, const std::uint8_t*, std::size_t,
    A2foStage8NebulaBytes*, A2foStage8NebulaBytes*);
using A2foStage8NebulaMapEmit = void (*)(
    void*, const std::uint8_t*, std::size_t,
    const std::uint8_t*, std::size_t);
void a2fo_rs_c_nebula_specular(
    const A2foStage8NebulaBytes*, std::size_t,
    const A2foStage8NebulaBytes*, std::size_t,
    bool, A2foStage8NebulaFold,
    A2foStage8NebulaMapStep, A2foStage8NebulaMapEmit,
    void*, const std::uint8_t*);
using A2foStage8NebulaBumpStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, const std::uint8_t*,
    std::size_t, std::uintptr_t, A2foStage8NebulaBytes*);
std::uint32_t a2fo_rs_c_nebula_apply_bump(
    bool, bool, std::size_t,
    A2foStage8NebulaBumpStep, void*, const std::uint8_t*);
using A2foStage8NebulaDamageStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uint32_t, std::size_t,
    const std::uint8_t*, std::size_t,
    A2foStage8NebulaBytes*, float*);
void a2fo_rs_c_nebula_damage(
    A2foStage8NebulaDamageStep, void*, const std::uint8_t*);
void a2fo_rs_c_nebula_logos(
    A2foStage8NebulaDamageStep, void*, const std::uint8_t*);
using A2foStage8NebulaIndexStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, std::size_t,
    const std::uint8_t*, std::size_t, A2foStage8NebulaBytes*);
void a2fo_rs_c_nebula_index_loose(
    A2foStage8NebulaBytes, A2foStage8NebulaBytes, std::size_t,
    A2foStage8NebulaFold, A2foStage8NebulaIndexStep,
    void*, const std::uint8_t*);
using A2foStage8NebulaNativeStep = std::uintptr_t (*)(
    void*, std::uint32_t, const std::uint8_t*, std::size_t,
    std::uintptr_t);
bool a2fo_rs_c_nebula_native_exists(
    A2foStage8NebulaBytes, A2foStage8NebulaBytes, bool,
    A2foStage8NebulaFold, A2foStage8NebulaNativeStep,
    void*, const std::uint8_t*);
std::uintptr_t a2fo_rs_c_nebula_native_variant(
    A2foStage8NebulaBytes, bool, A2foStage8NebulaNativeStep,
    void*, const std::uint8_t*);
using A2foStage8NebulaNeutralStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uintptr_t);
bool a2fo_rs_c_nebula_arm_neutral(
    bool, bool, bool, bool,
    A2foStage8NebulaNeutralStep, void*, const std::uint8_t*);
using A2foStage8NebulaListStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, A2foStage8NebulaBytes*);
void a2fo_rs_c_nebula_parameter_list(
    std::uintptr_t, std::uintptr_t, bool,
    A2foStage8NebulaListStep, void*, const std::uint8_t*,
    const std::uint8_t*);
using A2foStage8NebulaRootStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, A2foStage8NebulaBytes*);
void a2fo_rs_c_nebula_cache_roots(
    bool, A2foStage8NebulaRootStep, void*, const std::uint8_t*);
using A2foStage8NebulaFileStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::int64_t*);
bool a2fo_rs_c_nebula_read_text(
    A2foStage8NebulaFileStep, void*, const std::uint8_t*);
struct A2foStage8NebulaArtReport {
    std::uint32_t valid;
    std::uint32_t invalid;
};
using A2foStage8NebulaArtStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, A2foStage8NebulaArtReport*);
void a2fo_rs_c_nebula_load_art(
    A2foStage8NebulaArtStep, void*, const std::uint8_t*);
using A2foStage8NebulaSettingStep = std::uintptr_t (*)(
    void*, std::uint32_t);
void a2fo_rs_c_nebula_load_neutral_policy(
    A2foStage8NebulaSettingStep, void*, const std::uint8_t*);
bool a2fo_rs_c_nebula_load_native_bump(
    A2foStage8NebulaSettingStep, void*, const std::uint8_t*);
struct A2foStage8NebulaClassRow {
    A2foStage8NebulaBytes diffuse;
    A2foStage8NebulaBytes paths[6];
};
using A2foStage8NebulaClassStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, std::size_t,
    const std::uint8_t*, std::size_t,
    A2foStage8NebulaBytes*, A2foStage8NebulaBytes*);
using A2foStage8NebulaClassRegister = std::uintptr_t (*)(
    void*, std::uint32_t, const A2foStage8NebulaClassRow*,
    std::size_t, std::size_t);
void a2fo_rs_c_nebula_register_class(
    bool, bool, A2foStage8NebulaFold,
    A2foStage8NebulaClassStep, A2foStage8NebulaClassRegister,
    void*, const std::uint8_t*);
struct A2foStage8NebulaWalkOutput {
    std::uintptr_t child;
    std::uintptr_t sibling;
    A2foStage8NebulaBytes name;
};
using A2foStage8NebulaWalkStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uintptr_t, std::uintptr_t,
    A2foStage8NebulaWalkOutput*);
std::uintptr_t a2fo_rs_c_nebula_walk(
    std::uintptr_t, A2foStage8NebulaBytes, bool,
    A2foStage8NebulaFold, A2foStage8NebulaWalkStep,
    void*, const std::uint8_t*);
using A2foStage8NebulaInitStep = std::intptr_t (*)(
    void*, std::uint32_t, std::intptr_t);
bool a2fo_rs_c_nebula_init(
    bool, A2foStage8NebulaInitStep, void*, const std::uint8_t*);
void a2fo_rs_c_nebula_shutdown(
    A2foStage8NebulaInitStep, void*, const std::uint8_t*);
using A2foStage8NebulaRaceStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::size_t, A2foStage8NebulaBytes*);
void a2fo_rs_c_nebula_race_loaded(
    std::size_t, bool, A2foStage8NebulaRaceStep,
    void*, const std::uint8_t*);
}
