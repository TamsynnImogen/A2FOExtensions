#define CINTERFACE
#define COBJMACROS
#include "../renderer/d3d11/compressed_texture.hpp"
#include "../renderer/d3d11/texture_handle.hpp"
#include <d3d11.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace a2fo::renderer;
using namespace a2fo::renderer::textures;

void check(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
constexpr UINT all_support = D3D11_FORMAT_SUPPORT_TEXTURE2D |
    D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_MIP;

struct Fixture;
struct Texture {
    const ID3D11Texture2DVtbl* lpVtbl;
    Fixture* owner;
    unsigned refs = 0;
};
struct View {
    const ID3D11ShaderResourceViewVtbl* lpVtbl;
    Fixture* owner;
    unsigned refs = 0;
    unsigned serial = 0;
};
struct Device {
    const ID3D11DeviceVtbl* lpVtbl;
    Fixture* owner;
};
static_assert(offsetof(Device, lpVtbl) == 0 && offsetof(Texture, lpVtbl) == 0 &&
              offsetof(View, lpVtbl) == 0);

enum class Fault { none, feature_low, check_fail, support_low, texture_fail,
                   texture_partial, view_fail, view_partial, check_alloc,
                   check_runtime, texture_alloc, texture_runtime, texture_lifetime };
struct Fixture {
    Device device{};
    Texture texture{};
    View views[2]{};
    CompressedImage* mutating_image = nullptr;
    Fault fault = Fault::none;
    bool mutate_bytes = false, mutate_format = false, s_false = false;
    unsigned next_view = 0;
    unsigned exception_refs = 0;
    std::string exception_destroy_event = "exception:destroy";
    std::vector<std::string> events;
    explicit Fixture(const ID3D11DeviceVtbl* dv, const ID3D11Texture2DVtbl* tv,
                     const ID3D11ShaderResourceViewVtbl* vv) {
        device = {dv, this}; texture = {tv, this};
        views[0] = {vv, this, 0, 0}; views[1] = {vv, this, 0, 1};
    }
    void reset(Fault f = Fault::none) {
        check(texture.refs == 0 && views[0].refs == 0 && views[1].refs == 0,
              "all prior COM references released");
        check(exception_refs == 0, "all prior exception objects destroyed");
        fault = f; mutate_bytes = mutate_format = s_false = false;
        mutating_image = nullptr; events.clear();
    }
    void expect(const std::vector<std::string>& wanted, const char* case_name) const {
        if (events != wanted) {
            std::fprintf(stderr, "FAIL: %s event order\n", case_name);
            for (const auto& event : events) std::fprintf(stderr, "  %s\n", event.c_str());
            std::exit(1);
        }
    }
};
struct FixtureException {
    Fixture* fixture;
    explicit FixtureException(Fixture& f) : fixture(&f) { ++fixture->exception_refs; }
    FixtureException(const FixtureException& other) : fixture(other.fixture) {
        ++fixture->exception_refs;
    }
    FixtureException(FixtureException&& other) noexcept : fixture(other.fixture) {
        ++fixture->exception_refs;
    }
    ~FixtureException() {
        if (--fixture->exception_refs == 0)
            fixture->events.emplace_back(std::move(fixture->exception_destroy_event));
    }
};
Fixture& owner(ID3D11Device* p) { return *reinterpret_cast<Device*>(p)->owner; }
ID3D11Device* ptr(Device& d) { return reinterpret_cast<ID3D11Device*>(&d); }
ID3D11Texture2D* ptr(Texture& t) { return reinterpret_cast<ID3D11Texture2D*>(&t); }
ID3D11ShaderResourceView* ptr(View& v) { return reinterpret_cast<ID3D11ShaderResourceView*>(&v); }

D3D_FEATURE_LEVEL STDMETHODCALLTYPE feature(ID3D11Device* p) {
    auto& f = owner(p); f.events.emplace_back("feature");
    if (f.mutate_format) {
        check(f.mutating_image != nullptr, "format mutation image");
        f.mutating_image->format = DXGI_FORMAT_BC1_UNORM;
    }
    return f.fault == Fault::feature_low ? D3D_FEATURE_LEVEL_10_0 : D3D_FEATURE_LEVEL_11_0;
}
HRESULT STDMETHODCALLTYPE format_support(ID3D11Device* p, DXGI_FORMAT format, UINT* out) {
    auto& f = owner(p);
    f.events.emplace_back("check:" + std::to_string(static_cast<UINT>(format)));
    if (f.fault == Fault::check_alloc) throw std::bad_alloc();
    if (f.fault == Fault::check_runtime) throw std::runtime_error("mock check");
    if (f.fault == Fault::check_fail) return E_FAIL;
    *out = f.fault == Fault::support_low ?
        static_cast<UINT>(D3D11_FORMAT_SUPPORT_TEXTURE2D) : all_support;
    if (f.mutate_bytes) {
        check(f.mutating_image != nullptr, "byte mutation image");
        auto replacement = std::vector<std::uint8_t>(f.mutating_image->bytes.size(), 0xab);
        f.mutating_image->bytes.swap(replacement);
    }
    return f.s_false ? S_FALSE : S_OK;
}
HRESULT STDMETHODCALLTYPE create_texture(ID3D11Device* p, const D3D11_TEXTURE2D_DESC* d,
                                          const D3D11_SUBRESOURCE_DATA* sub,
                                          ID3D11Texture2D** out) {
    auto& f = owner(p); f.events.emplace_back("texture");
    check(f.mutating_image != nullptr, "image available for descriptor checks");
    const auto& image = *f.mutating_image;
    const DXGI_FORMAT expected = f.mutate_format ? DXGI_FORMAT_BC7_UNORM : image.format;
    check(d && sub && d->Width == image.width && d->Height == image.height &&
          d->MipLevels == image.mips.size() && d->ArraySize == 1 && d->Format == expected &&
          d->SampleDesc.Count == 1 && d->SampleDesc.Quality == 0 &&
          d->Usage == D3D11_USAGE_IMMUTABLE && d->BindFlags == D3D11_BIND_SHADER_RESOURCE &&
          d->CPUAccessFlags == 0 && d->MiscFlags == 0, "complete Texture2D descriptor");
    for (std::size_t i = 0; i < image.mips.size(); ++i) {
        const auto& mip = image.mips[i];
        check(sub[i].pSysMem == image.bytes.data() + mip.offset &&
              sub[i].SysMemPitch == mip.row_pitch &&
              sub[i].SysMemSlicePitch == mip.slice_pitch, "every subresource current base/offset/pitch");
        if (f.mutate_bytes)
            check(*static_cast<const std::uint8_t*>(sub[i].pSysMem) == 0xab,
                  "replacement bytes read");
    }
    if (f.fault == Fault::texture_alloc) throw std::bad_alloc();
    if (f.fault == Fault::texture_runtime) throw std::runtime_error("mock texture");
    if (f.fault == Fault::texture_fail) return E_FAIL;
    check(f.texture.refs == 0, "fresh texture slot");
    f.texture.refs = 1; *out = ptr(f.texture);
    if (f.fault == Fault::texture_lifetime) throw FixtureException(f);
    return f.fault == Fault::texture_partial ? E_FAIL : (f.s_false ? S_FALSE : S_OK);
}
HRESULT STDMETHODCALLTYPE create_view(ID3D11Device* p, ID3D11Resource* resource,
                                       const D3D11_SHADER_RESOURCE_VIEW_DESC* d,
                                       ID3D11ShaderResourceView** out) {
    auto& f = owner(p); f.events.emplace_back("view");
    check(resource == reinterpret_cast<ID3D11Resource*>(ptr(f.texture)) &&
          f.texture.refs == 1, "view receives live texture");
    const auto& image = *f.mutating_image;
    const DXGI_FORMAT expected = f.mutate_format ? DXGI_FORMAT_BC7_UNORM : image.format;
    check(d && d->Format == expected && d->ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D &&
          d->Texture2D.MostDetailedMip == 0 && d->Texture2D.MipLevels == image.mips.size(),
          "complete Texture2D SRV descriptor and mip range");
    const auto* raw = reinterpret_cast<const unsigned char*>(&d->Texture2D);
    for (std::size_t i = sizeof(d->Texture2D);
         i < sizeof(*d) - offsetof(D3D11_SHADER_RESOURCE_VIEW_DESC, Texture2D); ++i)
        check(raw[i] == 0, "unused SRV union bytes zero");
    if (f.fault == Fault::view_fail) return E_FAIL;
    View& v = f.views[f.next_view++ % 2];
    check(v.refs == 0, "fresh view slot"); v.refs = 1; *out = ptr(v);
    return f.fault == Fault::view_partial ? E_FAIL : (f.s_false ? S_FALSE : S_OK);
}
ULONG STDMETHODCALLTYPE release_texture(ID3D11Texture2D* p) {
    auto& t = *reinterpret_cast<Texture*>(p); t.owner->events.emplace_back("release:texture");
    check(t.refs == 1, "texture release exactly once"); return --t.refs;
}
ULONG STDMETHODCALLTYPE addref_view(ID3D11ShaderResourceView* p) {
    auto& v = *reinterpret_cast<View*>(p);
    v.owner->events.emplace_back("addref:view" + std::to_string(v.serial));
    check(v.refs > 0, "AddRef live view"); return ++v.refs;
}
ULONG STDMETHODCALLTYPE release_view(ID3D11ShaderResourceView* p) {
    auto& v = *reinterpret_cast<View*>(p);
    v.owner->events.emplace_back("release:view" + std::to_string(v.serial));
    check(v.refs > 0, "Release live view"); return --v.refs;
}

CompressedImage image(DXGI_FORMAT format, UINT width = 9, UINT height = 7,
                      unsigned levels = 3) {
    CompressedImage result; result.format = format; result.width = width;
    result.height = height; result.alpha_mode = AlphaMode::straight;
    for (unsigned i = 0; i < levels; ++i) {
        UINT row = 0, slice = 0;
        check(compressed_mip_layout(format, width, height, row, slice), "fixture mip layout");
        result.mips.push_back({width, height, row, slice, result.bytes.size()});
        result.bytes.insert(result.bytes.end(), slice, static_cast<std::uint8_t>(i + 1));
        width = std::max(1u, width / 2); height = std::max(1u, height / 2);
    }
    return result;
}
CompressedTextureInfo sentinel() {
    CompressedTextureInfo info; info.width = 101; info.height = 102;
    info.mip_levels = 103; info.block_payload_bytes = 104;
    info.rgba8_reference_bytes = 105; info.format = DXGI_FORMAT_R8_UNORM;
    info.alpha_mode = AlphaMode::custom; return info;
}
void unchanged(const CompressedTextureInfo& i) {
    check(i.width == 101 && i.height == 102 && i.mip_levels == 103 &&
          i.block_payload_bytes == 104 && i.rgba8_reference_bytes == 105 &&
          i.format == DXGI_FORMAT_R8_UNORM && i.alpha_mode == AlphaMode::custom,
          "info unchanged on failure");
}
std::string failure_text(const char* operation, HRESULT hr, const CompressedImage& i) {
    char text[360];
    std::snprintf(text, sizeof(text),
        "[Texture] %s HRESULT=0x%08lx DXGI_format=%u size=%ux%u mips=%zu compressed_bytes=%zu",
        operation, static_cast<unsigned long>(hr), static_cast<UINT>(i.format),
        i.width, i.height, i.mips.size(), i.bytes.size());
    return text;
}
void upload(Fixture& f, CompressedImage& i, Fault fault, HRESULT wanted,
            const char* operation, const std::vector<std::string>& events) {
    f.reset(fault); f.mutating_image = &i;
    auto info = sentinel(); std::string error = "stale";
    ID3D11ShaderResourceView* out = ptr(f.views[0]);
    const HRESULT hr = create_compressed_texture(ptr(f.device), i, &out, info, error);
    check(hr == wanted && out == nullptr, "failed upload HRESULT and cleared output");
    unchanged(info);
    if (operation) check(error == failure_text(operation, wanted, i), "precise failure text");
    else check(error == (wanted == E_OUTOFMEMORY
                        ? "[Texture] Compressed texture upload allocation failed"
                        : "[Texture] Compressed texture upload failed"), "exception error text");
    f.expect(events, operation ? operation : "exception");
}
} // namespace

int main() {
    ID3D11DeviceVtbl dv{}; dv.GetFeatureLevel = feature;
    dv.CheckFormatSupport = format_support; dv.CreateTexture2D = create_texture;
    dv.CreateShaderResourceView = create_view;
    ID3D11Texture2DVtbl tv{}; tv.Release = release_texture;
    ID3D11ShaderResourceViewVtbl vv{}; vv.AddRef = addref_view; vv.Release = release_view;
    Fixture f(&dv, &tv, &vv);
    for (DXGI_FORMAT format : {DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC4_UNORM,
                               DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC7_UNORM}) {
        auto i = image(format); f.reset(); f.mutating_image = &i;
        auto info = sentinel(); std::string error = "stale";
        ID3D11ShaderResourceView* out = nullptr;
        check(create_compressed_texture(ptr(f.device), i, &out, info, error) == S_OK &&
              out != nullptr && error.empty(), "BC mipchain upload success");
        check(info.format == format && info.width == i.width && info.height == i.height &&
              info.mip_levels == i.mips.size() && info.block_payload_bytes == i.bytes.size() &&
              info.rgba8_reference_bytes == 4ull * (9 * 7 + 4 * 3 + 2 * 1) &&
              info.alpha_mode == i.alpha_mode, "successful info");
        const auto check_event = "check:" + std::to_string(static_cast<UINT>(format));
        f.expect(format == DXGI_FORMAT_BC1_UNORM
                 ? std::vector<std::string>{check_event, "texture", "view", "release:texture"}
                 : std::vector<std::string>{"feature", check_event, "texture", "view", "release:texture"},
                 "successful upload order");
        check(f.texture.refs == 0 && f.views[(f.next_view - 1) % 2].refs == 1,
              "view caller owns one reference after texture release");
        out->lpVtbl->Release(out);
        check(f.views[0].refs == 0 && f.views[1].refs == 0, "caller released view");
        std::puts("PASS platform-d3d11-format");
    }
    auto bc7 = image(DXGI_FORMAT_BC7_UNORM);
    const auto check_bc7 = "check:" + std::to_string(static_cast<UINT>(DXGI_FORMAT_BC7_UNORM));
    f.events.reserve(16);
    f.reset();
    { auto info = sentinel(); std::string error;
      check(create_compressed_texture(ptr(f.device), bc7, nullptr, info, error) == E_POINTER &&
            error == failure_text("missing output view", E_POINTER, bc7),
            "missing upload output");
      unchanged(info); f.expect({}, "missing output has no device call"); }
    f.reset();
    { auto info = sentinel(); std::string error;
      ID3D11ShaderResourceView* out = ptr(f.views[0]);
      check(create_compressed_texture(nullptr, bc7, &out, info, error) == E_INVALIDARG &&
            out == nullptr &&
            error == failure_text("missing D3D11 device", E_INVALIDARG, bc7),
            "missing upload device");
      unchanged(info); f.expect({}, "missing device has no device call"); }
    upload(f, bc7, Fault::feature_low, DXGI_ERROR_UNSUPPORTED,
           "compressed format requires higher feature level", {"feature"});
    upload(f, bc7, Fault::check_fail, E_FAIL, "CheckFormatSupport", {"feature", check_bc7});
    upload(f, bc7, Fault::support_low, DXGI_ERROR_UNSUPPORTED,
           "format lacks Texture2D/sample/mip support", {"feature", check_bc7});
    upload(f, bc7, Fault::texture_fail, E_FAIL, "CreateTexture2D (compressed, immutable)",
           {"feature", check_bc7, "texture"});
    upload(f, bc7, Fault::texture_partial, E_FAIL, "CreateTexture2D (compressed, immutable)",
           {"feature", check_bc7, "texture", "release:texture"});
    upload(f, bc7, Fault::texture_lifetime, E_FAIL, nullptr,
           {"feature", check_bc7, "texture", "release:texture", "exception:destroy"});
    upload(f, bc7, Fault::view_fail, E_FAIL, "CreateShaderResourceView (all compressed mips)",
           {"feature", check_bc7, "texture", "view", "release:texture"});
    const std::string release_view0 = "release:view" + std::to_string((f.next_view) % 2);
    upload(f, bc7, Fault::view_partial, E_FAIL, "CreateShaderResourceView (all compressed mips)",
           {"feature", check_bc7, "texture", "view", release_view0, "release:texture"});
    for (auto fault : {Fault::check_alloc, Fault::texture_alloc})
        upload(f, bc7, fault, E_OUTOFMEMORY, nullptr,
               fault == Fault::check_alloc ? std::vector<std::string>{"feature", check_bc7}
                   : std::vector<std::string>{"feature", check_bc7, "texture"});
    for (auto fault : {Fault::check_runtime, Fault::texture_runtime})
        upload(f, bc7, fault, E_FAIL, nullptr,
               fault == Fault::check_runtime ? std::vector<std::string>{"feature", check_bc7}
                   : std::vector<std::string>{"feature", check_bc7, "texture"});
    std::puts("PASS platform-d3d11-failures");
    f.reset(); f.mutating_image = &bc7; f.s_false = true;
    { CompressedTextureInfo info; std::string error; ID3D11ShaderResourceView* out = nullptr;
      check(create_compressed_texture(ptr(f.device), bc7, &out, info, error) == S_OK && out,
            "S_FALSE COM calls accepted"); out->lpVtbl->Release(out); }
    f.reset(); f.mutating_image = &bc7; f.mutate_bytes = true;
    { CompressedTextureInfo info; std::string error; ID3D11ShaderResourceView* out = nullptr;
      check(create_compressed_texture(ptr(f.device), bc7, &out, info, error) == S_OK && out,
            "current image byte base after callback mutation"); out->lpVtbl->Release(out); }
    bc7 = image(DXGI_FORMAT_BC7_UNORM); f.reset(); f.mutating_image = &bc7; f.mutate_format = true;
    { CompressedTextureInfo info; std::string error; ID3D11ShaderResourceView* out = nullptr;
      check(create_compressed_texture(ptr(f.device), bc7, &out, info, error) == S_OK && out &&
            info.format == DXGI_FORMAT_BC7_UNORM, "format snapshot in descriptor plan");
      f.expect({"feature", "check:" + std::to_string(static_cast<UINT>(DXGI_FORMAT_BC1_UNORM)),
                "texture", "view", "release:texture"}, "current format check");
      out->lpVtbl->Release(out); }
    std::puts("PASS platform-d3d11-mutation");
    auto i = image(DXGI_FORMAT_BC1_UNORM);
    f.reset(); f.mutating_image = &i;
    f.next_view = 0;
    { CompressedTextureHandle handle; ID3D11ShaderResourceView* out = ptr(f.views[0]);
      check(handle.get_view(nullptr) == E_POINTER, "missing get_view output");
      check(handle.get_view(&out) == E_UNEXPECTED && out == nullptr, "uninitialized handle");
      std::string error;
      check(handle.initialize(ptr(f.device), i, error) == S_OK, "handle initialize");
      check(handle.get_view(&out) == S_OK && out && f.views[0].refs == 2,
            "get_view AddRef"); out->lpVtbl->Release(out);
      f.fault = Fault::view_fail;
      check(handle.initialize(ptr(f.device), i, error) == E_FAIL && f.views[0].refs == 1,
            "failed reinitialize preserves old view");
      check(handle.get_view(&out) == S_OK && out == ptr(f.views[0]), "old view remains accessible");
      out->lpVtbl->Release(out);
      f.fault = Fault::none;
      check(handle.initialize(ptr(f.device), i, error) == S_OK && f.views[0].refs == 0 &&
            f.views[1].refs == 1, "successful replacement releases old view once");
      check(handle.get_view(&out) == S_OK && out == ptr(f.views[1]), "new view accessible");
      out->lpVtbl->Release(out);
    }
    check(f.views[0].refs == 0 && f.views[1].refs == 0 && f.texture.refs == 0,
          "handle destructor releases final view");
    std::puts("PASS platform-d3d11-handle");
}
