#include "ui_renderer.hpp"
#include "compressed_texture.hpp"

#include "../state/fvf.hpp"
#include "../textures/upload.hpp"
#include "../shaders/bytecode_cache.hpp"
#include "../shaders/ui_fixed_function.hpp"
#include "../../rust/ui_renderer_ffi.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <new>
#include <type_traits>
#include <vector>
#include <utility>

namespace a2fo::renderer {
namespace {

constexpr HRESULT invalid_call = static_cast<HRESULT>(0x8876086c);
constexpr UINT max_buffer = 64u * 1024u * 1024u;
constexpr std::size_t max_states = 256;

template<class F> struct UiCall {
    F& function;
    std::exception_ptr error;
    static void invoke(void* opaque, std::uint32_t op, std::uint32_t index,
                       A2foRsUiReply* reply) noexcept {
        auto& self = *static_cast<UiCall*>(opaque);
        try { reply->value = self.function(op, index); }
        catch (...) { self.error = std::current_exception(); reply->failed = 1; }
    }
};
template<class F> HRESULT run_ui(std::uint32_t program, F&& function) {
    UiCall<std::remove_reference_t<F>> call{function, {}};
    const HRESULT result = a2fo_rs_ui_run(program, &call, &UiCall<std::remove_reference_t<F>>::invoke);
    if (call.error) std::rethrow_exception(call.error);
    return result;
}

template<class T> void release(T*& object) noexcept {
    if (object) { object->Release(); object = nullptr; }
}

template<class T> bool same_device(T* object, ID3D11Device* expected) {
    if (!object) return true;
    ID3D11Device* owner = nullptr;
    object->GetDevice(&owner);
    const bool same = owner == expected;
    release(owner);
    return same;
}

template<class T> void clear_cache(std::map<std::string, T*>& cache) noexcept {
    for (auto& item : cache) release(item.second);
    cache.clear();
}

template<class T> std::string descriptor_key(const T& descriptor) {
    return std::string(reinterpret_cast<const char*>(&descriptor), sizeof(descriptor));
}

bool used_texture(DWORD op, DWORD first, DWORD second) noexcept {
    return a2fo_rs_ui_used_texture(op, first, second) != 0;
}

bool blend_factor(DWORD value, bool alpha, D3D11_BLEND& factor) noexcept {
    const auto mapped = a2fo_rs_ui_blend(value, alpha);
    factor = static_cast<D3D11_BLEND>(mapped);
    return mapped != 0;
}

bool valid_state(const UiDrawState& state, const compat::FvfLayout& layout,
                 bool has_texture) noexcept {
    const std::uint32_t fields[]{state.color_op,state.color_arg1,state.color_arg2,
        state.alpha_op,state.alpha_arg1,state.alpha_arg2,state.alpha_test,state.alpha_blend,
        state.depth_enable,state.depth_write,state.specular_enable,state.alpha_ref,
        state.alpha_func,state.src_blend,state.dst_blend,state.blend_op,
        state.color_write_mask,state.depth_func,state.cull,state.min_filter,
        state.mag_filter,state.mip_filter,state.address_u,state.address_v};
    return a2fo_rs_ui_state(fields,layout.texture_count,has_texture) != 0;
}

bool validate_positions(const void* vertices, UINT count, UINT stride) noexcept {
    return a2fo_rs_ui_positions(static_cast<const std::uint8_t*>(vertices),count,stride) != 0;
}

bool valid_upload(const compat::TextureUpload& upload) noexcept {
    return a2fo_rs_ui_upload(upload.width,upload.height,upload.format,upload.row_pitch,
                             upload.bytes.size()) != 0;
}

struct alignas(16) DrawConstants {
    float viewport[4]{};
    std::uint32_t color[4]{};
    std::uint32_t alpha[4]{};
    std::uint32_t control[4]{};
    float unused[4]{};
};

} // namespace

struct UiRenderer::Impl {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* target = nullptr;
    ID3D11DepthStencilView* depth = nullptr;
    ID3D11ShaderResourceView* texture = nullptr;
    ID3D11PixelShader* pixel = nullptr;
    ID3D11Buffer* vertex_buffer = nullptr;
    ID3D11Buffer* index_buffer = nullptr;
    ID3D11Buffer* constants = nullptr;
    UINT vertex_capacity = 0, index_capacity = 0;
    UINT target_width = 0, target_height = 0;
    DWORD owner = 0;
    Viewport viewport{};
    std::string shader_cache;
    Log log = nullptr;
    ID3D11VertexShader* vertex_shaders[8]{};
    ID3D11InputLayout* input_layouts[8]{};
    std::map<std::string, ID3D11BlendState*> blends;
    std::map<std::string, ID3D11DepthStencilState*> depths;
    std::map<std::string, ID3D11RasterizerState*> rasters;
    std::map<std::string, ID3D11SamplerState*> samplers;
    std::vector<std::uint32_t> fan_indices;

    ~Impl() {
        clear_cache(blends); clear_cache(depths); clear_cache(rasters);
        clear_cache(samplers);
        for (auto*& shader : vertex_shaders) release(shader);
        for (auto*& layout : input_layouts) release(layout);
        release(constants); release(index_buffer); release(vertex_buffer);
        release(pixel); release(texture); release(depth); release(target);
        release(context); release(device);
    }

    bool ready() const noexcept { return device && context && owner == GetCurrentThreadId(); }

    HRESULT make_buffer(ID3D11Buffer*& buffer, UINT& capacity, UINT required,
                        UINT binding, const void* source) {
        ID3D11Buffer* created = nullptr;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: return required;
            case 1: return capacity;
            case 2: {
                D3D11_BUFFER_DESC description{};
                description.ByteWidth = a2fo_rs_ui_capacity(required);
                description.Usage = D3D11_USAGE_DYNAMIC;
                description.BindFlags = binding;
                description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                return device->CreateBuffer(&description, nullptr, &created);
            }
            case 3:
                release(buffer); buffer = created; capacity = a2fo_rs_ui_capacity(required);
                return 1;
            case 4: return context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            case 5: std::memcpy(mapped.pData, source, required); return 1;
            case 6: context->Unmap(buffer, 0); return 1;
            default: return 0;
            }
        };
        return run_ui(0, callback);
    }

    HRESULT ensure_constants(const DrawConstants& value) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: return constants == nullptr;
            case 1: {
                D3D11_BUFFER_DESC description{};
                description.ByteWidth = sizeof(value);
                description.Usage = D3D11_USAGE_DYNAMIC;
                description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                return device->CreateBuffer(&description, nullptr, &constants);
            }
            case 2: return context->Map(constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            case 3: std::memcpy(mapped.pData, &value, sizeof(value)); return 1;
            case 4: context->Unmap(constants, 0); return 1;
            default: return 0;
            }
        };
        return run_ui(1, callback);
    }

    HRESULT ensure_vertex_shader(const compat::FvfLayout& layout) {
        const unsigned variant = (layout.diffuse ? 1u : 0u) |
                                 (layout.specular ? 2u : 0u) |
                                 (layout.texture_count ? 4u : 0u);
        ShaderCompileResult compiled;
        ID3D11VertexShader* shader = nullptr;
        ID3D11InputLayout* input = nullptr;
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: return vertex_shaders[variant] != nullptr;
            case 1: {
                std::string source = std::string("#define HAS_DIFFUSE ") +
                    (layout.diffuse ? "1\n" : "0\n") + "#define HAS_SPECULAR " +
                    (layout.specular ? "1\n" : "0\n") + "#define HAS_TEXCOORD " +
                    (layout.texture_count ? "1\n" : "0\n") + shaders::ui_vertex;
                return compile_shader({source, "VSMain", "vs_5_0",
                    D3DCOMPILE_ENABLE_STRICTNESS}, shader_cache, compiled);
            }
            case 2: if (log) log(compiled.diagnostics); return 1;
            case 3:
                if (log) {
                    char message[160]{};
                    std::snprintf(message, sizeof(message),
                        "D3D11 UI VS variant=%u cache_hit=%u compile_us=%llu",
                        variant, compiled.cache_hit ? 1u : 0u, compiled.compile_us);
                    log(message);
                }
                return 1;
            case 4: return device->CreateVertexShader(compiled.bytes.data(),
                compiled.bytes.size(), nullptr, &shader);
            case 5: {
                D3D11_INPUT_ELEMENT_DESC elements[4]{};
                UINT count = 0;
                elements[count++] = {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                    layout.position_offset, D3D11_INPUT_PER_VERTEX_DATA, 0};
                if (layout.diffuse) elements[count++] = {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM,
                    0, layout.diffuse_offset, D3D11_INPUT_PER_VERTEX_DATA, 0};
                if (layout.specular) elements[count++] = {"COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                    0, layout.specular_offset, D3D11_INPUT_PER_VERTEX_DATA, 0};
                if (layout.texture_count) elements[count++] = {"TEXCOORD", 0,
                    DXGI_FORMAT_R32G32_FLOAT, 0, layout.texture_offsets[0],
                    D3D11_INPUT_PER_VERTEX_DATA, 0};
                return device->CreateInputLayout(elements, count, compiled.bytes.data(),
                    compiled.bytes.size(), &input);
            }
            case 6: release(shader); return 1;
            case 7: vertex_shaders[variant] = shader; input_layouts[variant] = input; return 1;
            default: return 0;
            }
        };
        return run_ui(2, callback);
    }

    template<class T, class D, class Create>
    HRESULT cached(std::map<std::string, T*>& cache, const D& description,
                   T*& result, Create create) {
        const auto key = descriptor_key(description);
        auto found = cache.find(key);
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: return found != cache.end();
            case 1: return cache.size();
            case 2: clear_cache(cache); return 1;
            case 3: return create(&result);
            case 4:
                try { cache.emplace(key, result); }
                catch (...) { release(result); throw; }
                return 1;
            case 5: result = found->second; return 1;
            default: return 0;
            }
        };
        return run_ui(3, callback);
    }

    HRESULT state_objects(const UiDrawState& state, ID3D11BlendState*& blend,
                          ID3D11DepthStencilState*& depth_state,
                          ID3D11RasterizerState*& raster,
                          ID3D11SamplerState*& sampler) {
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: {
                D3D11_BLEND factor{};
                return blend_factor(state.src_blend, false, factor) &&
                    blend_factor(state.dst_blend, false, factor) &&
                    blend_factor(state.src_blend, true, factor) &&
                    blend_factor(state.dst_blend, true, factor);
            }
            case 1: {
                D3D11_BLEND_DESC b{};
                b.RenderTarget[0].BlendEnable = state.alpha_blend;
                b.RenderTarget[0].RenderTargetWriteMask = static_cast<UINT8>(state.color_write_mask);
                blend_factor(state.src_blend, false, b.RenderTarget[0].SrcBlend);
                blend_factor(state.dst_blend, false, b.RenderTarget[0].DestBlend);
                blend_factor(state.src_blend, true, b.RenderTarget[0].SrcBlendAlpha);
                blend_factor(state.dst_blend, true, b.RenderTarget[0].DestBlendAlpha);
                b.RenderTarget[0].BlendOp = static_cast<D3D11_BLEND_OP>(state.blend_op);
                b.RenderTarget[0].BlendOpAlpha = static_cast<D3D11_BLEND_OP>(state.blend_op);
                return cached(blends,b,blend,[&](ID3D11BlendState** out) {
                    return device->CreateBlendState(&b,out); });
            }
            case 2: {
                D3D11_DEPTH_STENCIL_DESC d{};
                d.DepthEnable = state.depth_enable;
                d.DepthWriteMask = state.depth_write ? D3D11_DEPTH_WRITE_MASK_ALL :
                                                       D3D11_DEPTH_WRITE_MASK_ZERO;
                d.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(state.depth_func);
                d.StencilReadMask=D3D11_DEFAULT_STENCIL_READ_MASK;
                d.StencilWriteMask=D3D11_DEFAULT_STENCIL_WRITE_MASK;
                d.FrontFace.StencilFailOp=d.FrontFace.StencilDepthFailOp=d.FrontFace.StencilPassOp=D3D11_STENCIL_OP_KEEP;
                d.FrontFace.StencilFunc=D3D11_COMPARISON_ALWAYS;
                d.BackFace=d.FrontFace;
                return cached(depths,d,depth_state,[&](ID3D11DepthStencilState** out) {
                    return device->CreateDepthStencilState(&d,out); });
            }
            case 3: {
                D3D11_RASTERIZER_DESC r{};
                r.FillMode = D3D11_FILL_SOLID;
                r.CullMode = static_cast<D3D11_CULL_MODE>(state.cull);
                r.DepthClipEnable = TRUE;
                return cached(rasters,r,raster,[&](ID3D11RasterizerState** out) {
                    return device->CreateRasterizerState(&r,out); });
            }
            case 4: {
                D3D11_SAMPLER_DESC s{};
                s.Filter = static_cast<D3D11_FILTER>(a2fo_rs_ui_filter(
                    state.min_filter,state.mag_filter,state.mip_filter));
                s.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(state.address_u);
                s.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(state.address_v);
                s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                s.MaxAnisotropy = 1;
                s.ComparisonFunc = D3D11_COMPARISON_NEVER;
                s.MinLOD = 0;
                s.MaxLOD = state.mip_filter == 0 ? 0 : D3D11_FLOAT32_MAX;
                return cached(samplers,s,sampler,[&](ID3D11SamplerState** out) {
                    return device->CreateSamplerState(&s,out); });
            }
            default: return 0;
            }
        };
        return run_ui(4, callback);
    }

    HRESULT draw(UINT primitive_type, UINT primitive_count, DWORD fvf,
                 const void* vertices, std::size_t vertex_bytes, UINT vertex_count,
                 UINT stride, const void* indices, std::size_t index_bytes,
                 UINT index_format, const UiDrawState& state, bool indexed) {
        compat::FvfLayout layout{};
        UINT source_count = 0;
        bool texture_used = false;
        ID3D11Resource* sampled = nullptr;
        ID3D11Resource* rendered = nullptr;
        ID3D11Resource* depth_resource = nullptr;
        const void* index_data = indices;
        UINT index_size = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        ID3D11BlendState* blend = nullptr;
        ID3D11DepthStencilState* depth_state = nullptr;
        ID3D11RasterizerState* raster = nullptr;
        ID3D11SamplerState* sampler = nullptr;
        UINT draw_count = 0;
        unsigned variant = 0;
        D3D11_VIEWPORT native_viewport{};
        const UINT offset = 0;
        const float factor[4]{};
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: return ready() && target && viewport.width && viewport.height;
            case 1:
                return FAILED(compat::decode_fvf(fvf, layout)) || !layout.transformed ||
                    layout.normal || layout.texture_count > 1 ||
                    (layout.texture_count && layout.texture_components[0] != 2) ||
                    stride < layout.stride ||
                    stride > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES ? invalid_call : S_OK;
            case 2: return FAILED(compat::primitive_vertex_count(primitive_type,primitive_count,
                source_count)) ? invalid_call : S_OK;
            case 3: return primitive_count != 0;
            case 4: return vertices && stride && vertex_count <= max_buffer / stride &&
                vertex_bytes >= static_cast<std::size_t>(vertex_count) * stride && vertex_count &&
                (indexed || vertex_count >= source_count) &&
                validate_positions(vertices,vertex_count,stride) &&
                valid_state(state,layout,texture != nullptr) &&
                (!state.depth_enable || depth);
            case 5:
                texture_used = used_texture(state.color_op,state.color_arg1,state.color_arg2) ||
                    used_texture(state.alpha_op,state.alpha_arg1,state.alpha_arg2);
                return texture_used;
            case 6: texture->GetResource(&sampled); return 1;
            case 7: target->GetResource(&rendered); return 1;
            case 8: return depth != nullptr;
            case 9: depth->GetResource(&depth_resource); return 1;
            case 10: return sampled == rendered || (depth_resource && sampled == depth_resource);
            case 11: release(sampled); return 1;
            case 12: release(rendered); return 1;
            case 13: release(depth_resource); return 1;
            case 14:
                fan_indices.clear(); index_data = indices; index_size = 0;
                format = DXGI_FORMAT_UNKNOWN; return 1;
            case 15: return indexed;
            case 16:
                if (!a2fo_rs_ui_indices(static_cast<const std::uint8_t*>(indices),index_bytes,
                                         index_format,source_count,vertex_count)) return 0;
                index_size = index_format == 101 ? 2u : 4u;
                format = index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
                return 1;
            case 17: return primitive_type == 6;
            case 18: {
                if (primitive_count > max_buffer / (3u * sizeof(std::uint32_t)))
                    return invalid_call;
                const HRESULT status = compat::expand_triangle_fan(primitive_count,
                    indexed ? indices : nullptr,indexed ? index_format : 102,
                    vertex_count,fan_indices);
                if (FAILED(status)) return status;
                index_data = fan_indices.data(); index_size = 4;
                format = DXGI_FORMAT_R32_UINT; indexed = true;
                return S_OK;
            }
            case 19: return ensure_vertex_shader(layout);
            case 20: return state_objects(state,blend,depth_state,raster,sampler);
            case 21: {
                DrawConstants values{};
                values.viewport[0] = static_cast<float>(viewport.x);
                values.viewport[1] = static_cast<float>(viewport.y);
                values.viewport[2] = static_cast<float>(viewport.width);
                values.viewport[3] = static_cast<float>(viewport.height);
                values.color[0] = state.color_op; values.color[1] = state.color_arg1;
                values.color[2] = state.color_arg2; values.color[3] = state.specular_enable;
                values.alpha[0] = state.alpha_op; values.alpha[1] = state.alpha_arg1;
                values.alpha[2] = state.alpha_arg2;
                values.control[0] = texture_used; values.control[1] = state.alpha_test;
                values.control[2] = state.alpha_ref; values.control[3] = state.alpha_func;
                return ensure_constants(values);
            }
            case 22: return make_buffer(vertex_buffer,vertex_capacity,vertex_count * stride,
                D3D11_BIND_VERTEX_BUFFER,vertices);
            case 23: return indexed;
            case 24: {
                if (primitive_type == 6) draw_count = primitive_count * 3;
                else draw_count = source_count;
                return make_buffer(index_buffer,index_capacity,draw_count * index_size,
                    D3D11_BIND_INDEX_BUFFER,index_data);
            }
            case 25:
                if (!indexed) draw_count = source_count;
                variant = (layout.diffuse ? 1u : 0u) | (layout.specular ? 2u : 0u) |
                    (layout.texture_count ? 4u : 0u);
                native_viewport.TopLeftX = static_cast<float>(viewport.x);
                native_viewport.TopLeftY = static_cast<float>(viewport.y);
                native_viewport.Width = static_cast<float>(viewport.width);
                native_viewport.Height = static_cast<float>(viewport.height);
                native_viewport.MinDepth = 0; native_viewport.MaxDepth = 1;
                return 1;
            case 26: context->OMSetRenderTargets(1,&target,depth); return 1;
            case 27: context->RSSetViewports(1,&native_viewport); return 1;
            case 28: context->IASetInputLayout(input_layouts[variant]); return 1;
            case 29: context->IASetVertexBuffers(0,1,&vertex_buffer,&stride,&offset); return 1;
            case 30: context->IASetIndexBuffer(index_buffer,format,0); return 1;
            case 31: context->IASetPrimitiveTopology(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(
                a2fo_rs_ui_topology(primitive_type))); return 1;
            case 32: context->VSSetShader(vertex_shaders[variant],nullptr,0); return 1;
            case 33:
                context->HSSetShader(nullptr,nullptr,0);
                context->DSSetShader(nullptr,nullptr,0);
                context->GSSetShader(nullptr,nullptr,0); return 1;
            case 34: context->VSSetConstantBuffers(0,1,&constants); return 1;
            case 35: context->PSSetShader(pixel,nullptr,0); return 1;
            case 36: context->PSSetConstantBuffers(0,1,&constants); return 1;
            case 37: {
                ID3D11ShaderResourceView* active = texture_used ? texture : nullptr;
                context->PSSetShaderResources(0,1,&active); return 1;
            }
            case 38: context->PSSetSamplers(0,1,&sampler); return 1;
            case 39: context->RSSetState(raster); return 1;
            case 40: context->OMSetBlendState(blend,factor,0xffffffffu); return 1;
            case 41: context->OMSetDepthStencilState(depth_state,0); return 1;
            case 42: context->DrawIndexed(draw_count,0,0); return 1;
            case 43: context->Draw(source_count,0); return 1;
            default: return 0;
            }
        };
        return run_ui(5, callback);
    }

};

UiRenderer::UiRenderer() = default;
UiRenderer::~UiRenderer() { shutdown(); }

HRESULT UiRenderer::initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                               const std::string& cache_directory, Log log) {
    ID3D11Device* context_device = nullptr;
    bool same = false;
    std::unique_ptr<Impl> implementation;
    ShaderCompileResult compiled;
    try {
        const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
            switch (op) {
            case 0: return impl_ && impl_->owner != GetCurrentThreadId();
            case 1: shutdown(); return 1;
            case 2: return device && context;
            case 3: return device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
            case 4: return context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
            case 5: context->GetDevice(&context_device); same = context_device == device; return 1;
            case 6: release(context_device); return 1;
            case 7: return same;
            case 8:
                implementation = std::make_unique<Impl>();
                implementation->device = device; implementation->context = context;
                device->AddRef(); context->AddRef(); return 1;
            case 9:
                implementation->owner = GetCurrentThreadId();
                implementation->shader_cache = cache_directory;
                implementation->log = log; return 1;
            case 10: return compile_shader({shaders::ui_pixel, "PSMain", "ps_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS},cache_directory,compiled);
            case 11: if (log) log(compiled.diagnostics); return 1;
            case 12:
                if (log) {
                    char message[160]{};
                    std::snprintf(message,sizeof(message),
                        "D3D11 UI PS cache_hit=%u compile_us=%llu",
                        compiled.cache_hit ? 1u : 0u, compiled.compile_us);
                    log(message);
                }
                return 1;
            case 13: return device->CreatePixelShader(compiled.bytes.data(),
                compiled.bytes.size(),nullptr,&implementation->pixel);
            case 14: impl_ = std::move(implementation); return 1;
            default: return 0;
            }
        };
        return run_ui(6, callback);
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

void UiRenderer::shutdown() noexcept { impl_.reset(); }

HRESULT UiRenderer::set_target(ID3D11RenderTargetView* target,
                               ID3D11DepthStencilView* depth, UINT width, UINT height) {
    D3D11_RENDER_TARGET_VIEW_DESC target_view{};
    D3D11_DEPTH_STENCIL_VIEW_DESC depth_view{};
    D3D11_TEXTURE2D_DESC description{};
    ID3D11Resource* resource = nullptr;
    ID3D11Texture2D* image = nullptr;
    const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
        switch (op) {
        case 0: return impl_ && impl_->ready();
        case 1: return target != nullptr;
        case 2: return !depth && !width && !height;
        case 3:
            release(impl_->target); release(impl_->depth);
            impl_->target_width = impl_->target_height = 0;
            impl_->viewport = {}; return 1;
        case 4: return width && height && width <= 16384 && height <= 16384;
        case 5: return same_device(target,impl_->device);
        case 6: return same_device(depth,impl_->device);
        case 7:
            target->GetDesc(&target_view);
            return target_view.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D &&
                target_view.Texture2D.MipSlice == 0;
        case 8: return depth != nullptr;
        case 9:
            depth->GetDesc(&depth_view);
            return depth_view.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D &&
                depth_view.Texture2D.MipSlice == 0;
        case 10: {
            target->GetResource(&resource);
            const HRESULT status = resource ? resource->QueryInterface(__uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(&image)) : E_FAIL;
            release(resource); return status;
        }
        case 11:
            image->GetDesc(&description); release(image);
            return description.Width == width && description.Height == height &&
                description.ArraySize == 1 && description.SampleDesc.Count == 1;
        case 13: {
            depth->GetResource(&resource);
            const HRESULT status = resource ? resource->QueryInterface(__uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(&image)) : E_FAIL;
            release(resource); return status;
        }
        case 14:
            image->GetDesc(&description); release(image);
            return description.Width == width && description.Height == height &&
                description.ArraySize == 1 && description.SampleDesc.Count == 1;
        case 15:
            target->AddRef(); if (depth) depth->AddRef();
            release(impl_->target); release(impl_->depth);
            impl_->target = target; impl_->depth = depth;
            impl_->target_width = width; impl_->target_height = height;
            impl_->viewport = {0,0,width,height,0,1}; return 1;
        default: return 0;
        }
    };
    return run_ui(7, callback);
}

HRESULT UiRenderer::set_viewport(const Viewport& viewport) {
    if (!impl_ || !impl_->ready() || !impl_->target) return invalid_call;
    const std::uint32_t fields[]{viewport.x,viewport.y,viewport.width,viewport.height};
    if (!a2fo_rs_ui_viewport(fields,viewport.min_z,viewport.max_z,
                             impl_->target_width,impl_->target_height)) return invalid_call;
    impl_->viewport = viewport;
    return S_OK;
}

HRESULT UiRenderer::set_texture(ID3D11ShaderResourceView* texture) {
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
        switch (op) {
        case 0: return impl_ && impl_->ready();
        case 1: return same_device(texture,impl_->device);
        case 2: return texture != nullptr;
        case 3:
            texture->GetDesc(&view);
            return view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D;
        case 4:
            if (texture) texture->AddRef();
            release(impl_->texture); impl_->texture = texture; return 1;
        default: return 0;
        }
    };
    return run_ui(8, callback);
}

HRESULT UiRenderer::upload_texture(const compat::TextureUpload& upload,
                                   ID3D11ShaderResourceView** output) {
    ID3D11Texture2D* image = nullptr;
    const auto callback = [&](std::uint32_t op, std::uint32_t) -> std::int64_t {
        switch (op) {
        case 0: return output != nullptr;
        case 1: *output = nullptr; return 1;
        case 2: return impl_ && impl_->ready();
        case 3: return valid_upload(upload);
        case 4: {
            D3D11_TEXTURE2D_DESC description{};
            description.Width = upload.width;
            description.Height = upload.height;
            description.MipLevels = 1; description.ArraySize = 1;
            description.Format = upload.format; description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = upload.bytes.data(); data.SysMemPitch = upload.row_pitch;
            return impl_->device->CreateTexture2D(&description,&data,&image);
        }
        case 5: return impl_->device->CreateShaderResourceView(image,nullptr,output);
        case 6: release(image); return 1;
        default: return 0;
        }
    };
    return run_ui(9, callback);
}

HRESULT UiRenderer::upload_compressed_texture(const textures::CompressedImage& image,
    ID3D11ShaderResourceView** output,std::string& error) {
    if(output)*output=nullptr;
    if(!output)return E_POINTER;
    if(!impl_||!impl_->ready()) {
        error="[Texture] Compressed upload requires initialized owner-thread renderer";
        return invalid_call;
    }
    CompressedTextureInfo info;
    return create_compressed_texture(impl_->device,image,output,info,error);
}

HRESULT UiRenderer::draw_up(UINT primitive_type, UINT primitive_count, DWORD fvf,
                            const void* vertices, std::size_t vertex_bytes, UINT stride,
                            const UiDrawState& state) {
    try {
        UINT count = 0;
        if (FAILED(compat::primitive_vertex_count(primitive_type, primitive_count,
                                                   count))) return invalid_call;
        return impl_ ? impl_->draw(primitive_type, primitive_count, fvf, vertices,
            vertex_bytes, count, stride, nullptr, 0, 0, state, false) : invalid_call;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT UiRenderer::draw_indexed_up(UINT primitive_type, UINT primitive_count, DWORD fvf,
                                    const void* vertices, std::size_t vertex_bytes,
                                    UINT vertex_count, UINT stride, const void* indices,
                                    std::size_t index_bytes, UINT index_format,
                                    const UiDrawState& state) {
    try {
        return impl_ ? impl_->draw(primitive_type, primitive_count, fvf, vertices,
            vertex_bytes, vertex_count, stride, indices, index_bytes, index_format,
            state, true) : invalid_call;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT UiRenderer::clear_color(DWORD argb, UINT count, const Rect* rects) {
    if (!impl_ || !impl_->ready() || !impl_->target || (count && !rects))
        return invalid_call;
    try {
        const LONG left = static_cast<LONG>(impl_->viewport.x);
        const LONG top = static_cast<LONG>(impl_->viewport.y);
        const LONG right = static_cast<LONG>(impl_->viewport.x + impl_->viewport.width);
        const LONG bottom = static_cast<LONG>(impl_->viewport.y + impl_->viewport.height);
        const std::int32_t bounds[]{left,top,right,bottom};
        UiDrawState state{};
        state.color_op = 1; state.alpha_op = 1;
        state.alpha_blend = false; state.depth_enable = false;
        state.depth_write = false; state.color_write_mask = 15;
        state.cull = 1;
        struct Vertex { float x, y, z, rhw; DWORD color; };
        const auto draw_rect = [&](const Rect& rect) -> HRESULT {
            const std::int32_t edges[]{rect.left,rect.top,rect.right,rect.bottom};
            float clipped[4]{};
            if (!a2fo_rs_ui_clip(bounds,edges,clipped)) return S_OK;
            const Vertex vertices[6] = {
                {clipped[0],clipped[1],0,1,argb},
                {clipped[2],clipped[1],0,1,argb},
                {clipped[0],clipped[3],0,1,argb},
                {clipped[0],clipped[3],0,1,argb},
                {clipped[2],clipped[1],0,1,argb},
                {clipped[2],clipped[3],0,1,argb}};
            return draw_up(4,2,0x44,vertices,sizeof(vertices),sizeof(Vertex),state);
        };
        const auto callback = [&](std::uint32_t op, std::uint32_t i) -> std::int64_t {
            switch (op) {
            case 0: return impl_ && impl_->ready() && impl_->target && (!count || rects);
            case 1: return count;
            case 2: return draw_rect({left,top,right,bottom});
            case 3: return rects[i].right >= rects[i].left &&
                rects[i].bottom >= rects[i].top;
            case 4: return draw_rect(rects[i]);
            default: return 0;
            }
        };
        return run_ui(10, callback);
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

} // namespace a2fo::renderer
