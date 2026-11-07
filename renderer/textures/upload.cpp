#include "upload.hpp"
#include "../../rust/textures_ffi.h"
#include <d3d8.h>
#include <new>
#include <stdexcept>

namespace a2fo::renderer::compat {
HRESULT prepare_texture_upload(UINT format, UINT width, UINT height,
                               const void* source, std::size_t source_bytes,
                               UINT source_pitch, TextureUpload& output) noexcept {
    if (!source) return D3DERR_INVALIDCALL;
    A2foRsUploadPlan plan{};
    if (!a2fo_rs_textures_upload_plan(format, width, height, source_bytes,
                                      source_pitch, &plan)) return D3DERR_INVALIDCALL;
    TextureUpload prepared;
    prepared.format = static_cast<DXGI_FORMAT>(plan.format);
    prepared.width = plan.width; prepared.height = plan.height;
    prepared.row_pitch = plan.row_pitch;
    if (plan.output_size > prepared.bytes.max_size()) return D3DERR_INVALIDCALL;
    try { prepared.bytes.resize(plan.output_size); }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const std::length_error&) { return D3DERR_INVALIDCALL; }
    if (!a2fo_rs_textures_upload_copy(format, width, height,
            static_cast<const std::uint8_t*>(source), plan.required_source,
            source_pitch, prepared.bytes.data(), prepared.bytes.size()))
        return D3DERR_INVALIDCALL;
    output.bytes.swap(prepared.bytes);
    output.format = prepared.format; output.width = prepared.width;
    output.height = prepared.height; output.row_pitch = prepared.row_pitch;
    return S_OK;
}
}
