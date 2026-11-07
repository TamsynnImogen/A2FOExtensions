#define CINTERFACE
#include "../../rust/d3d_native_ffi.h"
#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>

namespace {
template<class Function> Function method_pointer(std::uintptr_t address) noexcept {
    static_assert(sizeof(Function) == sizeof(address));
    Function function;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}
void remember(void* sink) noexcept {
    if (sink) *static_cast<std::exception_ptr*>(sink) = std::current_exception();
}

std::int32_t feature(std::uintptr_t address, void* object,
                     std::uint32_t* level, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11DeviceVtbl*)nullptr)->GetFeatureLevel);
        *level = method_pointer<Fn>(address)(static_cast<ID3D11Device*>(object));
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}
std::int32_t check_format(std::uintptr_t address, void* object,
                          std::uint32_t format, std::uint32_t* support,
                          std::int32_t* hr, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11DeviceVtbl*)nullptr)->CheckFormatSupport);
        *hr = method_pointer<Fn>(address)(static_cast<ID3D11Device*>(object),
                                         static_cast<DXGI_FORMAT>(format), support);
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}
std::int32_t create_texture(std::uintptr_t address, void* object,
                            const void* descriptor, const void* subresources,
                            void** texture, std::int32_t* hr, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11DeviceVtbl*)nullptr)->CreateTexture2D);
        *hr = method_pointer<Fn>(address)(static_cast<ID3D11Device*>(object),
            static_cast<const D3D11_TEXTURE2D_DESC*>(descriptor),
            static_cast<const D3D11_SUBRESOURCE_DATA*>(subresources),
            reinterpret_cast<ID3D11Texture2D**>(texture));
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}
std::int32_t create_view(std::uintptr_t address, void* object, void* resource,
                         const void* descriptor, void** view,
                         std::int32_t* hr, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11DeviceVtbl*)nullptr)->CreateShaderResourceView);
        *hr = method_pointer<Fn>(address)(static_cast<ID3D11Device*>(object),
            static_cast<ID3D11Resource*>(resource),
            static_cast<const D3D11_SHADER_RESOURCE_VIEW_DESC*>(descriptor),
            reinterpret_cast<ID3D11ShaderResourceView**>(view));
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}
std::int32_t addref_view(std::uintptr_t address, void* object, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11ShaderResourceViewVtbl*)nullptr)->AddRef);
        method_pointer<Fn>(address)(static_cast<ID3D11ShaderResourceView*>(object));
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}
std::int32_t release_view(std::uintptr_t address, void* object, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11ShaderResourceViewVtbl*)nullptr)->Release);
        method_pointer<Fn>(address)(static_cast<ID3D11ShaderResourceView*>(object));
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}
std::int32_t release_texture(std::uintptr_t address, void* object, void* sink) noexcept {
    try {
        using Fn = decltype(((ID3D11Texture2DVtbl*)nullptr)->Release);
        method_pointer<Fn>(address)(static_cast<ID3D11Texture2D*>(object));
        return 1;
    } catch (const std::bad_alloc&) { remember(sink); return 2; }
      catch (...) { remember(sink); return 0; }
}

constexpr std::size_t slot(std::size_t offset) noexcept {
    return offset / sizeof(void*);
}
constexpr std::size_t sdk_layout[] = {
    sizeof(D3D11_TEXTURE2D_DESC), alignof(D3D11_TEXTURE2D_DESC),
    offsetof(D3D11_TEXTURE2D_DESC, Width),
    offsetof(D3D11_TEXTURE2D_DESC, Height),
    offsetof(D3D11_TEXTURE2D_DESC, MipLevels),
    offsetof(D3D11_TEXTURE2D_DESC, ArraySize),
    offsetof(D3D11_TEXTURE2D_DESC, Format),
    offsetof(D3D11_TEXTURE2D_DESC, SampleDesc),
    offsetof(D3D11_TEXTURE2D_DESC, Usage),
    offsetof(D3D11_TEXTURE2D_DESC, BindFlags),
    offsetof(D3D11_TEXTURE2D_DESC, CPUAccessFlags),
    offsetof(D3D11_TEXTURE2D_DESC, MiscFlags),
    sizeof(DXGI_SAMPLE_DESC), alignof(DXGI_SAMPLE_DESC),
    offsetof(DXGI_SAMPLE_DESC, Count), offsetof(DXGI_SAMPLE_DESC, Quality),
    sizeof(D3D11_SUBRESOURCE_DATA), alignof(D3D11_SUBRESOURCE_DATA),
    offsetof(D3D11_SUBRESOURCE_DATA, pSysMem),
    offsetof(D3D11_SUBRESOURCE_DATA, SysMemPitch),
    offsetof(D3D11_SUBRESOURCE_DATA, SysMemSlicePitch),
    sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC), alignof(D3D11_SHADER_RESOURCE_VIEW_DESC),
    offsetof(D3D11_SHADER_RESOURCE_VIEW_DESC, Format),
    offsetof(D3D11_SHADER_RESOURCE_VIEW_DESC, ViewDimension),
    offsetof(D3D11_SHADER_RESOURCE_VIEW_DESC, Texture2D),
    sizeof(D3D11_TEX2D_SRV), alignof(D3D11_TEX2D_SRV),
    offsetof(D3D11_TEX2D_SRV, MostDetailedMip),
    offsetof(D3D11_TEX2D_SRV, MipLevels),
    slot(offsetof(ID3D11ShaderResourceViewVtbl, AddRef)),
    slot(offsetof(ID3D11ShaderResourceViewVtbl, Release)),
    slot(offsetof(ID3D11DeviceVtbl, CreateTexture2D)),
    slot(offsetof(ID3D11DeviceVtbl, CreateShaderResourceView)),
    slot(offsetof(ID3D11DeviceVtbl, CheckFormatSupport)),
    slot(offsetof(ID3D11DeviceVtbl, GetFeatureLevel)),
    D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE,
    D3D11_SRV_DIMENSION_TEXTURE2D,
};
static_assert(sizeof(sdk_layout) / sizeof(sdk_layout[0]) == 39);
const A2FoD3DGuardApi guards{feature, check_format, create_texture, create_view,
                             addref_view, release_view, release_texture};
} // namespace

extern "C" const A2FoD3DGuardApi* a2fo_native_d3d_guard_api() noexcept {
    std::size_t expected[39]{};
    if (a2fo_rs_d3d_native_layout(expected, 39) != 39) return nullptr;
    for (std::size_t i = 0; i < 39; ++i)
        if (expected[i] != sdk_layout[i]) return nullptr;
    return &guards;
}
