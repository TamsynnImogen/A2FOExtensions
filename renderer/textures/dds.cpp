#include "dds.hpp"
#include "../../rust/textures_ffi.h"
#include <cstring>
#include <new>
#include <stdexcept>

namespace a2fo::renderer::textures {
namespace {
const char* detail(UINT id) noexcept {
    static const char* const messages[] = {
        "", "truncated DDS header", "invalid DDS magic", "invalid DDS header size",
        "invalid DDS dimensions or flags", "DDS pixel format is not FourCC compressed",
        "conflicting DDS pixel format flags", "DDS cube or volume texture unsupported",
        "DDS mip count exceeds full chain", "truncated DX10 header",
        "DX10 resource is not a single 2D texture", "invalid DX10 misc or alpha flags",
        "unsupported DDS compression format", "DDS mip pitch overflow",
        "truncated DDS mip payload", "unsupported BC format", "invalid image dimensions",
        "invalid alpha mode", "invalid mip count", "inconsistent mip layout",
        "truncated mip payload", "extra mip payload bytes"
    };
    return id < sizeof(messages) / sizeof(messages[0]) ? messages[id] : "invalid DDS input";
}
HRESULT finish(UINT status, UINT error_id, std::string& error) {
    if (!status) { error.clear(); return S_OK; }
    error = detail(error_id);
    return status == 2 ? HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) : E_INVALIDARG;
}
}
bool compressed_mip_layout(DXGI_FORMAT format, UINT width, UINT height,
                           UINT& row_pitch, UINT& slice_pitch) noexcept {
    return a2fo_rs_textures_layout(static_cast<UINT>(format), width, height,
                                    &row_pitch, &slice_pitch) != 0;
}
HRESULT validate_compressed_image(const CompressedImage& image, std::string& error) noexcept {
    try {
        A2foRsMip mips[15]{};
        if (image.mips.size() <= 15)
            for (std::size_t i = 0; i < image.mips.size(); ++i)
                mips[i] = {image.mips[i].width, image.mips[i].height,
                           image.mips[i].row_pitch, image.mips[i].slice_pitch,
                           image.mips[i].offset};
        UINT id = 0;
        const UINT status = a2fo_rs_textures_validate(static_cast<UINT>(image.format),
            image.width, image.height, static_cast<UINT>(image.alpha_mode),
            image.mips.size() <= 15 ? mips : nullptr,
            image.mips.size(), image.bytes.size(), &id);
        return finish(status, id, error);
    } catch (const std::bad_alloc&) { error.clear(); return E_OUTOFMEMORY; }
      catch (...) { error.clear(); return E_INVALIDARG; }
}
HRESULT parse_compressed_dds(const void* data, std::size_t size,
                             CompressedImage& output, std::string& error) noexcept {
    try {
        A2foRsDdsPlan plan{};
        const UINT status = a2fo_rs_textures_parse(static_cast<const std::uint8_t*>(data), size, &plan);
        if (status) return finish(status, plan.error, error);
        CompressedImage image;
        image.format = static_cast<DXGI_FORMAT>(plan.format);
        image.width = plan.width; image.height = plan.height;
        image.alpha_mode = static_cast<AlphaMode>(plan.alpha);
        image.mips.reserve(plan.mip_count);
        for (UINT i = 0; i < plan.mip_count; ++i) {
            const auto& m = plan.mips[i];
            image.mips.push_back({m.width, m.height, m.row_pitch, m.slice_pitch, m.offset});
        }
        image.bytes.resize(plan.payload_size);
        std::memcpy(image.bytes.data(), static_cast<const std::uint8_t*>(data) + plan.payload_offset,
                    plan.payload_size);
        output.bytes.swap(image.bytes); output.mips.swap(image.mips);
        output.format = image.format; output.width = image.width; output.height = image.height;
        output.alpha_mode = image.alpha_mode;
        error.clear(); return S_OK;
    } catch (const std::bad_alloc&) { error.clear(); return E_OUTOFMEMORY; }
      catch (const std::length_error&) { error.clear(); return E_OUTOFMEMORY; }
      catch (...) { error.clear(); return E_INVALIDARG; }
}
}
