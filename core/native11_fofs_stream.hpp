#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
namespace a2fo::native11_fofs_stream {
// Qualify only the pinned Fleet search-root fopen replacement. This neither
// invokes it nor changes stream/file ownership or native loading.
bool qualify(HMODULE image,HMODULE fleet_ops) noexcept;
bool owned(HMODULE image,HMODULE fleet_ops) noexcept;
// Call after HIGHLOW normalization of the copied native profile region.
// Only the exact six-byte fopen CALL is restored to its on-disk comparison
// representation; all other bytes remain subject to their normal profile.
bool normalize(HMODULE image,HMODULE fleet_ops,std::uint32_t region_rva,
    std::uint8_t* bytes,std::size_t size) noexcept;
}
