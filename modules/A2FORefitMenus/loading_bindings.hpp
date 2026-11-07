#pragma once
#include <array>
#include <cstdint>

namespace a2fo::refit_menu::loading_bindings {
// Verified against the installed ArmadaL 817b1ddb... and FO 3fd18eee...,
// with independent pass34 evidence. These are RVAs in their respective images.
inline constexpr std::uint32_t new_bar = 0x1d58b8; // FO stdcall(float, device*, float)
inline constexpr std::array<std::uint8_t,6> new_bar_entry{{0x55,0x8b,0xec,0x83,0xc4,0xf0}};
inline constexpr std::uint32_t device_flush = 0x226440;
inline constexpr std::array<std::uint8_t,21> flush_body{{
    0x56,0x8b,0xf1,0xe8,0x68,0xb0,0xff,0xff,0x8d,0x8e,0xe4,0x00,0x00,0x00,
    0xe8,0x3d,0xb6,0xff,0xff,0x5e,0xc3}};
// Native loading loop resolves the active ST3D device, then calls vtable +38.
inline constexpr std::uint32_t flush_site = 0x198f36;
inline constexpr std::array<std::uint8_t,23> flush_site_code{{
    0xa1,0x08,0xd5,0x7a,0x00,0x8b,0x88,0xc0,0x00,0x00,0x00,
    0x8b,0x8c,0x88,0xcc,0x00,0x00,0x00,0x8b,0x11,0xff,0x52,0x38}};
inline constexpr std::uint32_t game_setup = 0x157940;
inline constexpr std::array<std::uint8_t,4> game_setup_code{{0x8b,0x41,0x30,0xc3}};
inline constexpr std::uint32_t map_name = 0x1461f0;
inline constexpr std::array<std::uint8_t,6> map_name_code{{0x56,0x8b,0xf1,0x8b,0x46,0x10}};
inline constexpr std::uint32_t map_title = 0x146240;
inline constexpr std::array<std::uint8_t,7> map_title_code{{0x8b,0x41,0x10,0x83,0xc0,0x6c,0xc3}};
inline constexpr std::uint32_t transport = 0x36b8d4;
// FO's native configuration and localised tip state, not a second ODF parser.
inline constexpr std::uint32_t bar_height = 0x246a70;
inline constexpr std::uint32_t tip_text = 0x246aac;
inline constexpr std::uint32_t tip_sprite = 0x246ab0;
// Cross-checked in installed ArmadaL, not assumed from Refit's stock addresses.
// Slot getters are thiscall with RET4; player_name has hidden string result,
// then slot (RET8). Destroy that result through the engine's own MSVCP60 IAT.
inline constexpr std::uint32_t local_slot=0x147470, player_race=0x146650,
    player_team=0x146610, player_vacant=0x146af0, game_player=0x146b90,
    player_name=0x1466e0, find_race=0x8b180, string_dtor_iat=0x3b7dc0,
    mission_race=0x337cc0;
inline constexpr std::uint32_t status_site=0x2019eb, status_update=0x198ba0;
inline constexpr std::array<std::uint8_t,5> status_call{{0xe8,0xb0,0x71,0xf9,0xff}};
// Revalidated against the same pinned ArmadaL image for loading frames between
// native 5% updates. The splash exit is hooked at its entry so every caller
// releases session resources before native splash/device cleanup.
inline constexpr std::uint32_t show_splash=0x198ff0, kill_splash=0x199210,
    status_visible=0x373be4, fraction_remaining=0x3a8828, compute_fraction=0x201ce0,
    splash_model=0x373bd8, float_one=0x2ae218;
inline constexpr std::array<std::uint8_t,5> splash_entry{{0x55,0x8b,0xec,0x6a,0xff}};
inline constexpr std::array<std::uint32_t,3> load_db_sites{{0x22cf2a,0x22cf44,0x22cf79}};
inline constexpr std::uint32_t load_db=0x22ccd0, load_pixels_site=0x2426e2, load_pixels=0x2434b0;
// ST3D_DeviceDirectX8 cleanup: called by DestroyRenderSurfaces and other
// native device teardown paths, including a display change during GameOpen.
// Signature includes the later Release and clearing of wrapper->device (+90).
inline constexpr std::uint32_t release_device=0x2239f0;
inline constexpr std::uint32_t fo_release_device=0x1fd0a0,fo_release_gateway=0x24a41c,fo_load_pixels=0x1f06d0;
inline constexpr std::array<std::uint8_t,25> fo_pixels_body{{
    0x55,0x8b,0xec,0x51,0x89,0x4d,0xfc,0x8b,0x45,0x08,0x50,0x8b,0x45,0xfc,0x50,
    0xe8,0x00,0xfa,0xff,0xff,0x59,0x5d,0xc2,0x04,0x00}};
inline constexpr std::array<std::uint8_t,9> release_entry{{0x55,0x8b,0xec,0x81,0xec,0xa8,0,0,0}};
inline constexpr std::array<std::uint8_t,35> release_tail{{
    0x8b,0x86,0x90,0,0,0,0x3b,0xc3,0x74,0x19,0x8b,0x10,0x53,0x50,0xff,0x52,0x30,
    0x8b,0x86,0x90,0,0,0,0x50,0x8b,0x08,0xff,0x51,0x08,0x89,0x9e,0x90,0,0,0}};
}
