#pragma once
#include <cstdint>

extern "C" {
// Synchronous native primitives; UINTPTR_MAX stops the Rust workflow. Native
// Transaction refund op 8 writes its raw u32 event kind through the argument
// pointer and returns metadata zero, so UINT32_MAX remains ordinary data.
// Native wrappers retain exception_ptrs and rethrow only after Rust returns. Values,
// strings, locks and map entries remain owned by their native contexts.
using A2foStage10ResourcesStep = std::uintptr_t (*)(void*, std::uint32_t, std::uintptr_t);
std::uint8_t a2fo_rs_stage10_resources_transaction(void*, A2foStage10ResourcesStep, std::uint32_t mode);
std::uint8_t a2fo_rs_stage10_resources_locked(void*, A2foStage10ResourcesStep, std::uint32_t mode);
std::uint8_t a2fo_rs_stage10_resources_public(void*, A2foStage10ResourcesStep, std::uint32_t kind);
std::uint8_t a2fo_rs_stage10_resources_panel(void*, A2foStage10ResourcesStep);
std::uint8_t a2fo_rs_stage10_resources_cache(void*, A2foStage10ResourcesStep, std::uint32_t kind);
std::uint8_t a2fo_rs_stage10_resources_tooltip(void*, A2foStage10ResourcesStep);
std::uint8_t a2fo_rs_stage10_resources_event(void*, A2foStage10ResourcesStep, std::uint32_t kind);
std::uint8_t a2fo_rs_stage10_resources_init(void*, A2foStage10ResourcesStep);
std::uint8_t a2fo_rs_stage10_resources_lookup(void*, A2foStage10ResourcesStep, std::uint32_t kind);
std::uint8_t a2fo_rs_stage10_resources_key(void*, A2foStage10ResourcesStep, std::uint32_t resource, std::uint32_t kind);
std::uint8_t a2fo_rs_stage10_resources_added_valid(std::uintptr_t index, std::uint32_t kind);
}
