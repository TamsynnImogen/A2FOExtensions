#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {
using A2foStage8Lower = std::uint8_t (*)(void*, std::uint8_t);
using A2foStage8Space = std::uint8_t (*)(void*, std::uint8_t);
using A2foStage8Include = void (*)(void*, const std::uint8_t*, std::size_t);
struct A2foStage8Bytes {
    const std::uint8_t* data;
    std::size_t size;
};
using A2foStage8Load = std::uint8_t (*)(
    void*, A2foStage8Bytes, A2foStage8Bytes*);
using A2foStage8ShieldStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uintptr_t);
using A2foStage8ClassStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uintptr_t, A2foStage8Bytes*);
using A2foStage8IndexStep = std::uint8_t (*)(
    void*, std::uint32_t, std::uint32_t, A2foStage8Bytes,
    A2foStage8Bytes, A2foStage8Bytes*, std::uint32_t*);

// Returns the required output length, excluding the optional terminator.
std::size_t a2fo_rs_stage8_shield_normalize(
    const std::uint8_t* input, std::size_t input_size,
    std::uint8_t* output, std::size_t output_capacity,
    A2foStage8Lower lower, void* context);

// 0 = absent, 1 = false, 2 = true. Includes are emitted in source order.
std::uint8_t a2fo_rs_stage8_shield_parse(
    const std::uint8_t* contents, std::size_t contents_size,
    A2foStage8Lower lower, A2foStage8Space space,
    A2foStage8Include include, void* context);

// Resolve local flags before includes, and later includes before earlier ones.
std::uint8_t a2fo_rs_stage8_shield_resolve(
    const std::uint8_t* name, std::size_t name_size,
    A2foStage8Lower lower, A2foStage8Space space,
    A2foStage8Load load, void* context);

void a2fo_rs_stage8_shield_update(
    std::uint8_t ready, std::uintptr_t craft,
    A2foStage8ShieldStep step, void* context);

std::uint8_t a2fo_rs_stage8_shield_class_policy(
    std::uintptr_t object_class, A2foStage8ClassStep step, void* context);

std::uint8_t a2fo_rs_stage8_shield_parameter(
    const std::uint8_t* value, std::size_t value_size,
    A2foStage8Lower lower, A2foStage8Space space, void* context);

void a2fo_rs_stage8_shield_register(
    std::uint8_t ready, std::uintptr_t object_class,
    std::uintptr_t parameter_db, A2foStage8ShieldStep step, void* context);

void a2fo_rs_stage8_shield_scan(A2foStage8ShieldStep step, void* context);

std::uint8_t a2fo_rs_stage8_shield_activation(
    std::uint64_t activation, std::uint64_t previous,
    A2foStage8ShieldStep step, void* context);

std::uint8_t a2fo_rs_stage8_shield_init(
    A2foStage8ShieldStep step, void* context);

void a2fo_rs_stage8_shield_shutdown(
    A2foStage8ShieldStep step, void* context);

void a2fo_rs_stage8_shield_render(
    std::uint8_t ready, A2foStage8ShieldStep step, void* context);

void a2fo_rs_stage8_shield_cleanup(
    std::uint8_t ready, std::uintptr_t craft,
    A2foStage8ShieldStep step, void* context);

void a2fo_rs_stage8_shield_create(
    std::uintptr_t craft, A2foStage8ShieldStep step, void* context);

void a2fo_rs_stage8_shield_remove(
    std::uintptr_t craft, A2foStage8ShieldStep step, void* context);

std::uint8_t a2fo_rs_stage8_shield_install_publish(
    A2foStage8ShieldStep step, void* context);

std::uintptr_t a2fo_rs_stage8_shield_after_gateway(
    std::uintptr_t object, std::uint8_t require_object,
    A2foStage8ShieldStep step, void* context);

std::uint8_t a2fo_rs_stage8_shield_index(
    A2foStage8Lower lower, A2foStage8IndexStep step, void* context);
}
