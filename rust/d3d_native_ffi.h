#pragma once
#include "d3d_resources_ffi.h"

// Native D3D11 bridge. Method addresses are read from the SDK vtable by Rust;
// these callbacks invoke the exact SDK function pointer types inside C++
// exception firewalls. Status: 1 = invoked, 2 = std::bad_alloc, 0 = other
// exception. HRESULT outputs are meaningful only when status is 1.
// The opaque exception sink is a caller-owned C++ std::exception_ptr that
// remains valid for the synchronous Rust call. A null sink discards exceptions.
extern "C" {
struct A2FoD3DGuardApi {
    std::int32_t (*feature)(std::uintptr_t method, void* object,
                            std::uint32_t* level, void* exception_sink) noexcept;
    std::int32_t (*check_format)(std::uintptr_t method, void* object,
                                 std::uint32_t format, std::uint32_t* support,
                                 std::int32_t* hr, void* exception_sink) noexcept;
    std::int32_t (*create_texture)(std::uintptr_t method, void* object,
                                   const void* descriptor, const void* subresources,
                                   void** texture, std::int32_t* hr,
                                   void* exception_sink) noexcept;
    std::int32_t (*create_view)(std::uintptr_t method, void* object,
                                void* resource, const void* descriptor,
                                void** view, std::int32_t* hr,
                                void* exception_sink) noexcept;
    std::int32_t (*addref_view)(std::uintptr_t method, void* object,
                                void* exception_sink) noexcept;
    std::int32_t (*release_view)(std::uintptr_t method, void* object,
                                 void* exception_sink) noexcept;
    std::int32_t (*release_texture)(std::uintptr_t method, void* object,
                                    void* exception_sink) noexcept;
};
struct A2FoD3DNativeHost {
    void* context;
    std::int32_t (*validate)(void* context) noexcept;
    // 0 = current image.format; 1 = current image.bytes.data().
    std::uintptr_t (*read)(void* context, std::uint32_t field) noexcept;
    void* exception_sink;
};

// Rust writes entries when capacity equals the required count, and returns that count.
// A mismatch in any of the 39 SDK layout values disables the guard table.
std::size_t a2fo_rs_d3d_native_layout(std::size_t* values,
                                      std::size_t capacity) noexcept;
const A2FoD3DGuardApi* a2fo_native_d3d_guard_api() noexcept;
// Status: 1 = completed, 0 = bridge failure, 2 = bad_alloc, 3 = other exception.
std::int32_t a2fo_rs_d3d_native_upload(const A2FoD3DImage* image,
    const A2FoD3DConstants* constants, void* device, std::int32_t has_output,
    const A2FoD3DNativeHost* host, const A2FoD3DGuardApi* guards,
    void** view_out, A2FoD3DResult* result) noexcept;
using A2FoD3DNativeCreate = std::int32_t (*)(void*,void**) noexcept;
std::int32_t a2fo_rs_d3d_native_handle_initialize(void**,const A2FoD3DGuardApi*,
    void*,A2FoD3DNativeCreate) noexcept;
std::int32_t a2fo_rs_d3d_native_handle_get(const void*,void**) noexcept;
void a2fo_rs_d3d_native_handle_drop(void*) noexcept;
}
