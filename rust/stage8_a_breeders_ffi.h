#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foStage8BreederBytes {
    const std::uint8_t* data;
    std::size_t size;
};
struct A2foStage8BreederPolicy {
    std::uint8_t breeder;
    std::uint8_t mother;
    float mother_radius;
    float consume_radius;
    A2foStage8BreederBytes seed;
    A2foStage8BreederBytes result;
    A2foStage8BreederBytes produces;
};
using A2foStage8BreederLower = std::uint8_t (*)(void*, std::uint8_t);
using A2foStage8BreederField = std::uint8_t (*)(
    void*, std::uint32_t, A2foStage8BreederBytes*, A2foStage8BreederBytes*);
using A2foStage8BreederFloat = std::uint8_t (*)(
    void*, A2foStage8BreederBytes, float*);
using A2foStage8BreederStore = std::uint8_t (*)(
    void*, std::uintptr_t, const A2foStage8BreederPolicy*);
using A2foStage8BreederStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uintptr_t);
using A2foStage8BreederEmit = void (*)(
    void*, const std::uint8_t*, std::size_t);

void a2fo_rs_stage8_breeder_class_loaded(
    std::uintptr_t object_class, std::uint32_t field_count,
    A2foStage8BreederLower lower, A2foStage8BreederField field,
    A2foStage8BreederFloat parse_float, A2foStage8BreederStore store,
    void* context);

std::uint8_t a2fo_rs_stage8_breeder_init(
    A2foStage8BreederStep step, void* context);
void a2fo_rs_stage8_breeder_shutdown(
    A2foStage8BreederStep step, void* context);

std::uint8_t a2fo_rs_stage8_breeder_producer_event(
    std::uintptr_t event, std::uint32_t required_size,
    std::uint32_t admit_kind, A2foStage8BreederStep step, void* context);

std::uint8_t a2fo_rs_stage8_breeder_activation(
    std::uint64_t current, std::uint64_t previous,
    A2foStage8BreederStep step, void* context);

void a2fo_rs_stage8_breeder_craft_event(
    std::uintptr_t event, std::uint32_t required_size,
    std::uint32_t cleanup_kind, std::uint32_t postload_kind,
    std::uint32_t simulate_kind, std::uint8_t builtin,
    A2foStage8BreederStep step, void* context);

std::uint8_t a2fo_rs_stage8_breeder_begin_request(
    std::uintptr_t breeder, std::uintptr_t target_class,
    A2foStage8BreederStep step, void* context);

std::uintptr_t a2fo_rs_stage8_breeder_nearest_mother(
    std::uintptr_t breeder, A2foStage8BreederStep step, void* context);

void a2fo_rs_stage8_breeder_odf_key(
    const std::uint8_t* value, std::size_t size,
    A2foStage8BreederLower lower, A2foStage8BreederEmit emit,
    void* context);

std::uint8_t a2fo_rs_stage8_breeder_metamorphose(
    std::uintptr_t breeder, A2foStage8BreederStep step, void* context);

void a2fo_rs_stage8_breeder_update_waiting(
    std::uintptr_t breeder, A2foStage8BreederStep step, void* context);

void a2fo_rs_stage8_breeder_update_active(
    std::uintptr_t breeder, A2foStage8BreederStep step, void* context);
}
