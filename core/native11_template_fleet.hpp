#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
namespace a2fo::native11_template_fleet {
// Read-only admission of the exact Fleet database-constructor chain.
bool qualify(HMODULE image,HMODULE fleet_ops) noexcept;
bool owned(HMODULE image,HMODULE fleet_ops) noexcept;
// Restore only the reviewed parser-constructor block in a comparison copy,
// after the caller has normalized its HIGHLOW operands. No game bytes change.
bool normalize(HMODULE image,HMODULE fleet_ops,std::uint32_t rva,
    std::uint8_t* bytes,std::size_t size) noexcept;
}
