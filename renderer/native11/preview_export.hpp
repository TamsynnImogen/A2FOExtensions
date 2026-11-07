#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {

struct PreviewExportStatus {
    std::string state;
    std::string reason;
    std::uint64_t serial=0;
    std::uint64_t frame=0;
    std::uint64_t source_epoch=0;
    std::uint32_t source_ticket=0;
    std::uint32_t objects=0;
    std::uint32_t width=0;
    std::uint32_t height=0;
    char slot=0;
};

// Input is tightly packed, top-down BGRA8. Output is a top-down BMP24.
// On failure, output is unchanged and error describes the rejected input.
bool encode_preview_bmp(const std::vector<std::uint8_t>& bgra,
    std::uint32_t width,std::uint32_t height,
    std::vector<std::uint8_t>& output,std::string& error) noexcept;

// 64-bit counters are JSON strings so JavaScript readers retain their values.
bool encode_preview_status(const PreviewExportStatus& status,
    std::string& output,std::string& error) noexcept;

} // namespace a2fo::native11
