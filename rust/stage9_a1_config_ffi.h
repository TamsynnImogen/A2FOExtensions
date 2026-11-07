#ifndef A2FO_STAGE9_A1_CONFIG_FFI_H
#define A2FO_STAGE9_A1_CONFIG_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {
using A2foA1ConfigNumber = bool (*)(const std::uint8_t*, std::size_t,
                                     std::size_t*, double*);
using A2foA1ConfigRound = bool (*)(double, std::int32_t*);
using A2foA1ConfigStep = bool (*)(void*, std::uint32_t, std::uint32_t,
                                   const std::uint8_t**, std::size_t*);
using A2foA1ConfigClassify = std::uint8_t (*)(std::uint8_t);
using A2foA1ConfigLine = bool (*)(void*, const std::uint8_t*,
                                  std::size_t, const std::uint8_t**,
                                  std::size_t*);
using A2foA1ConfigInteger = bool (*)(void*, std::size_t,
                                     std::size_t*, std::int32_t*);
using A2foA1ConfigCommit = void (*)(void*, const std::int32_t*);
using A2foA1GameplayStep = std::uint32_t (*)(void*, std::uint32_t,
                                             std::uint32_t);

bool a2fo_rs_stage9_a1_config_literal(
    const std::uint8_t*, std::size_t, std::size_t, std::int32_t*,
    A2foA1ConfigNumber, A2foA1ConfigRound);
std::uint32_t a2fo_rs_stage9_a1_config_apply(
    const std::uint8_t*, std::size_t, std::int32_t*,
    A2foA1ConfigNumber, A2foA1ConfigRound);
std::uint32_t a2fo_rs_stage9_a1_config_defaults(
    void*, std::uint32_t, A2foA1ConfigStep);
bool a2fo_rs_stage9_a1_config_ini(
    const std::uint8_t*, std::size_t, bool*,
    A2foA1ConfigClassify, A2foA1ConfigClassify);
bool a2fo_rs_stage9_a1_config_ini_roots(
    void*, std::uint32_t, bool*, A2foA1ConfigStep,
    A2foA1ConfigClassify, A2foA1ConfigClassify);
bool a2fo_rs_stage9_a1_config_gui_rect(
    const std::uint8_t*, std::size_t, const std::uint8_t*,
    std::size_t, void*, A2foA1ConfigLine,
    A2foA1ConfigInteger, A2foA1ConfigCommit);
bool a2fo_rs_stage9_a1_config_gameplay(
    void*, std::uint32_t, A2foA1GameplayStep);
}

#endif
