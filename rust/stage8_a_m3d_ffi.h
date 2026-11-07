#pragma once

#include <cstdint>

extern "C" std::uint8_t a2fo_rs_stage8_m3d_init(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_m3d_shutdown(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_m3d_resolve(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_m3d_name(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind);
extern "C" void a2fo_rs_stage8_m3d_join(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_m3d_index(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint32_t a2fo_rs_stage8_m3d_remove_caches(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_m3d_load(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_m3d_read_file(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint32_t a2fo_rs_stage8_m3d_parser_stage(
    std::uintptr_t consumed, std::uintptr_t materials,
    std::uintptr_t nodes, std::uintptr_t animations,
    std::uintptr_t references);
extern "C" void a2fo_rs_stage8_m3d_report(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_m3d_finalize(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_m3d_materialize(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_m3d_bind_textures(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_m3d_find_texture(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_m3d_hierarchy_audit(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_m3d_restore_names(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_m3d_inspect_tree(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_m3d_cleanup_database(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
