#pragma once
#include "bindings.hpp"

// Four supplied FO images agree at these sites (including October Roots).
// Hook lengths end at complete instructions and contain no address operands.
namespace a2fo::refit_menu::native {
inline constexpr std::uint32_t bitmap_load_rva = 0x16cfa0;
inline constexpr std::uint32_t bitmap32_load_rva = 0x16d278;
inline constexpr std::uint32_t image_paint_rva = 0xa43e0;
inline constexpr std::array<std::uint32_t, 3> shell_draw_rvas{{0x1f8db4, 0x1f8dfc, 0x1f8e44}};
inline constexpr std::size_t background_field = 0x37c;
inline constexpr std::size_t image_picture = 0x198, picture_graphic = 0x0c;
inline constexpr std::uint32_t bitmap_vmt_rva = 0x8d15c;
inline constexpr std::size_t bitmap32_width = 0x44, bitmap32_height = 0x40, bitmap32_bits = 0x54;
inline constexpr Signature skin_signatures[]{
    {0x16cfa0,"\x55\x8b\xec\x81\xc4\xe0\xfe\xff\xff",9},
    {0x16d278,"\x55\x8b\xec\x33\xc9\x51",6},
    {0x94000,"\x8b\x40\x28\x8b\x40\x1c\xc3",7},
    {0x93e78,"\x8b\xc8\x8b\x41\x28\x8b\x40\x20\x99\x33\xc2\x2b\xc2\xc3",14},
    {0x93de0,"\x53\x56\x8b\xd8\x83\x7b\x2c\x00\x75\x35",10},
    {0x8f538,"\x53\x8b\xd8\x8b\xc3\x8b\x10\xff\x52\x10",10},
    {0x94dd0,"\x55\x8b\xec\x83\xc4\xa0\x53\x56\x57\x8b\xda\x89\x45\xfc\x8b\x45\xfc",17},
    {0xec004,"\x0f\xaf\x50\x44\x8b\x40\x54\x8d\x04\x90\xc3",11},
    {0xec120,"\x56\x8b\xf0\x80\xbe\xc0\x00\x00\x00\x00\x0f\x85\x8a\x00\x00\x00\x8b\x4e\x54\x83\xc1\x03\x8b\x46\x44\x0f\xaf\x46\x40",29},
    {0x1cbe1e,"\x8b\x83\x7c\x03\x00\x00\x8b\x80\x98\x01\x00\x00\xe8\x79\x57\xec\xff",17},
    {0xa44d0,"\x8b\x45\xfc\x8b\x80\x98\x01\x00\x00\x8b\x48\x0c",12},
    {0xa43e0,"\x55\x8b\xec\x83\xc4\xe8",6},
    // GetParentForm follows TControl.Parent at +30 (a-ad04, a-ad0c).
    {0xaad0c,"\x8b\x76\x30\x84\xdb",5},
    {0x1f8db4,"\x55\x8b\xec\x51\x8b\x45\xfc",7},
    {0x1f8dfc,"\x55\x8b\xec\x51\x8b\x45\xfc",7},
    {0x1f8e44,"\x55\x8b\xec\x51\x8b\x45\xfc",7},
    {0xa420c,"\x53\x56\x57\x55\x83\xc4\xe8\x8b\xea\x8b\xd8",11},
};
}
