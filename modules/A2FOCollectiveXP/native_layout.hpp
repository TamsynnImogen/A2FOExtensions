#pragma once
#include <array>
#include <cstdint>

// Verified against the Jan_B FleetOpsHook image; see docs/collective-xp.md.
namespace a2fo::collective_xp::native {
inline constexpr std::uint32_t award_rva = 0x1dcfc3;
inline constexpr std::uint32_t level_rva = 0x1dbda4;
inline constexpr std::uint32_t sleeper_award_rva = 0x13722c;
inline constexpr std::uint32_t saboteur_award_rva = 0x1387dd;
inline constexpr std::array<std::uint8_t, 13> special_award_bytes{
    0xd9,0x80,0xb4,0,0,0,0x58,0xd8,0x00,0x58,0xd9,0x18,0x9b};
inline constexpr std::uint32_t team_save_rva = 0x10e9c4;
inline constexpr std::uint32_t team_load_rva = 0x10ea90;
inline constexpr std::uint32_t clean_teams_rva = 0x96300;
inline constexpr std::array<std::uint8_t, 16> award_bytes{
    0xd9,0x40,0x10,0x8b,0x52,0x04,0xd8,0x82,0xb4,0,0,0,0xd9,0x58,0x10,0x9b};
inline constexpr std::array<std::uint8_t, 6> level_bytes{0x55,0x8b,0xec,0x83,0xc4,0x88};
inline constexpr std::array<std::uint8_t, 6> save_bytes{0x55,0x8b,0xec,0x83,0xc4,0xf4};
inline constexpr std::array<std::uint8_t, 6> load_bytes = save_bytes;
inline constexpr std::array<std::uint8_t, 7> clean_bytes{0x56,0x57,0xbf,0xb0,0x8d,0x73,0};
inline constexpr std::uint32_t fleetops_text_crc = 0x5e621788;
inline constexpr std::uint32_t fleetops_september30_text_crc = 0xca18aa0c;
// October 12 paired DLL SHA-256 3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11.
// Its changed branch is outside the XP/promotion/team persistence sites.
inline constexpr std::uint32_t fleetops_october12_text_crc = 0x28c90d19;
}
