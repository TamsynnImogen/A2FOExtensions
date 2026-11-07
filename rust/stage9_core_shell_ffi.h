#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t (*A2foRsShellStep)(void*, std::uint32_t, std::uint64_t);
std::uint64_t a2fo_rs_stage9_shell_flow(std::uint32_t, A2foRsShellStep, void*, const std::uint8_t*);
std::uint8_t a2fo_rs_stage9_shell_settings_valid(std::int32_t, std::int32_t, std::int32_t);
std::uint8_t a2fo_rs_stage9_shell_rect_equal(const std::int32_t*, const std::int32_t*);
std::uint64_t a2fo_rs_stage9_shell_styles(std::uint8_t, std::uint32_t, std::uint32_t, const std::uint32_t*);
std::uint64_t a2fo_rs_stage9_shell_client(std::int32_t, std::int32_t);
void a2fo_rs_stage9_shell_center(const std::int32_t*, const std::int32_t*, std::int32_t*);
}
