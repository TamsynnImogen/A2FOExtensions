// Whole-source CPU fixture: no real D3D device or texture is constructed.
// CORE_SOURCE is an absolute generated copy of the frozen or current renderer.
#include <windows.h>
#include <d3d8.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include CORE_SOURCE

static_assert(sizeof(SparseEmissivePixel) == 8);

static int sparse_allocation_hook = 0;
static std::uint32_t* sparse_mutation_pixel = nullptr;

__attribute__((noinline, noipa)) void* operator new(std::size_t size) {
    if (size == sizeof(SparseEmissivePixel)) {
        if (sparse_allocation_hook == 3) {
            sparse_allocation_hook = 0;
            throw std::bad_alloc();
        }
        if (sparse_allocation_hook == 1 || sparse_allocation_hook == 2) {
            *sparse_mutation_pixel = 0x00abcdefu;
            sparse_allocation_hook = sparse_allocation_hook == 2 ? 4 : 0;
        }
    } else if (size == 2 * sizeof(SparseEmissivePixel) &&
               sparse_allocation_hook == 4) {
        sparse_allocation_hook = 0;
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}

namespace a2fo::renderer::game {
std::string legacy_source_path(IDirect3DDevice8*, const char*) noexcept {
    return {};
}
}

namespace {

void check(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL:%s\n", label);
        std::abort();
    }
}

struct FakeTexture {
    void** vtable;
    UINT width = 1, height = 1;
    DWORD levels = 1;
    std::uint32_t rows[3][32]{};
    UINT pitch_pixels = 4;
    int fail_lock = -1;
    bool null_bits = false;
    unsigned locks = 0, unlocks = 0, releases = 0;
};

struct FakeDevice { void** vtable; };
void* texture_vtable[19]{};
void* device_vtable[97]{};
FakeTexture source_texture{texture_vtable};
FakeTexture composite_texture{texture_vtable};
FakeTexture black_texture{texture_vtable};
FakeDevice device{device_vtable};
FakeTexture* create_target = &composite_texture;
bool loader_fails = false;
unsigned loader_calls = 0, create_calls = 0;

ULONG WINAPI fake_release(void* self) {
    ++static_cast<FakeTexture*>(self)->releases;
    return 1;
}
DWORD WINAPI fake_level_count(void* self) {
    return static_cast<FakeTexture*>(self)->levels;
}
HRESULT WINAPI fake_level_desc(void* self, UINT level, D3DSURFACE_DESC* out) {
    auto& texture = *static_cast<FakeTexture*>(self);
    if (level >= texture.levels) return E_FAIL;
    std::memset(out, 0, sizeof(*out));
    out->Width = texture.width;
    out->Height = texture.height;
    return S_OK;
}
HRESULT WINAPI fake_lock(void* self, UINT level, D3DLOCKED_RECT* out,
                         const RECT*, DWORD) {
    auto& texture = *static_cast<FakeTexture*>(self);
    ++texture.locks;
    if (static_cast<int>(level) == texture.fail_lock) return E_FAIL;
    if (level >= 3) return E_FAIL;
    out->Pitch = static_cast<INT>(texture.pitch_pixels * sizeof(std::uint32_t));
    out->pBits = texture.null_bits ? nullptr : texture.rows[level];
    return S_OK;
}
HRESULT WINAPI fake_unlock(void* self, UINT) {
    ++static_cast<FakeTexture*>(self)->unlocks;
    return E_FAIL; // Caller intentionally ignores UnlockRect failure.
}
HRESULT WINAPI fake_create(void*, UINT width, UINT height, UINT levels,
                           DWORD, D3DFORMAT, D3DPOOL,
                           IDirect3DTexture8** output) {
    ++create_calls;
    create_target->width = width;
    create_target->height = height;
    create_target->levels = levels ? levels : 2;
    *output = reinterpret_cast<IDirect3DTexture8*>(create_target);
    return S_OK;
}
HRESULT WINAPI fake_load(IDirect3DDevice8*, LPCSTR, UINT, UINT, UINT, DWORD,
                         D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
                         void*, PALETTEENTRY*, IDirect3DTexture8** output) {
    ++loader_calls;
    *output = loader_fails ? nullptr :
        reinterpret_cast<IDirect3DTexture8*>(&source_texture);
    return loader_fails ? E_FAIL : S_OK;
}

void reset() {
    sparse_allocation_hook = 0;
    sparse_mutation_pixel = nullptr;
    source_texture = FakeTexture{texture_vtable};
    composite_texture = FakeTexture{texture_vtable};
    black_texture = FakeTexture{texture_vtable};
    create_target = &composite_texture;
    loader_fails = false;
    loader_calls = create_calls = 0;
    g_create_texture_from_file = fake_load;
    g_black_emissive_texture = nullptr;
    g_emissive_composite_cache_bytes = 0;
    g_emissive_composite_use_clock = 0;
    g_logged_emissive_create_failure = 0;
    g_logged_emissive_loader_unavailable = 0;
    g_log = [](const std::string&) {};
}

void test_mips() {
    reset();
    composite_texture.levels = 3;
    composite_texture.pitch_pixels = 5;
    const std::vector<std::uint32_t> pixels{
        0x00000000, 0x01010101, 0x02020202, 0x03030303,
        0x04040404, 0x05050505, 0x06060606, 0x07070707,
        0x08080808, 0x09090909, 0x0a0a0a0a, 0x0b0b0b0b};
    check(upload_emissive_mip_chain(
              reinterpret_cast<IDirect3DTexture8*>(&composite_texture),
              pixels, 4, 3), "mip success");
    for (UINT y = 0; y < 3; ++y)
        for (UINT x = 0; x < 4; ++x)
            check(composite_texture.rows[0][y * 5 + x] == pixels[y * 4 + x],
                  "mip base pitched copy");
    check(composite_texture.rows[1][0] == 0x03030303,
          "mip first rounded channel");
    check(composite_texture.rows[1][1] == 0x05050505,
          "mip second rounded channel");
    check(composite_texture.rows[2][0] == 0x04040404,
          "mip one by one repeat");
    check(composite_texture.unlocks == 3, "mip unlock count");
    std::puts("mip-even-odd-pitched:PASS");

    reset();
    composite_texture.levels = 2;
    check(upload_emissive_mip_chain(
              reinterpret_cast<IDirect3DTexture8*>(&composite_texture),
              {0x80808080}, 1, 1), "single pixel success");
    check(composite_texture.rows[1][0] == 0x80808080,
          "single pixel repeated sample");
    check(!upload_emissive_mip_chain(nullptr, {1}, 1, 1), "null texture");
    check(!upload_emissive_mip_chain(
              reinterpret_cast<IDirect3DTexture8*>(&composite_texture),
              {1, 2}, 1, 1), "pixel count mismatch");
    std::puts("mip-single-preconditions:PASS");

    for (bool null_bits : {false, true}) {
        reset();
        composite_texture.levels = 2;
        composite_texture.fail_lock = null_bits ? -1 : 0;
        composite_texture.null_bits = null_bits;
        check(!upload_emissive_mip_chain(
                  reinterpret_cast<IDirect3DTexture8*>(&composite_texture),
                  {1}, 1, 1), "mip lock failure");
        check(composite_texture.unlocks == 0, "failed lock not unlocked");
    }
    std::puts("mip-lock-failures:PASS");
}

void test_mip_raw_bit_matrix() {
    struct Golden {
        UINT width, height;
        std::uint64_t hash[3];
    };
    // Precomputed independently from the native implementation. Each hash
    // covers all three levels, in row-major pixel order including alpha.
    constexpr Golden cases[] = {
        {1, 1, {0xc4832c63cf8be874ull, 0x90e9a47bffdbc56bull, 0xdcb2a46afc92f745ull}},
        {1, 3, {0x93fc666d54296f59ull, 0x285dd6659450308aull, 0x5c685c35fc9ddfbcull}},
        {3, 1, {0x93fc666d54296f59ull, 0x285dd6659450308aull, 0x5c685c35fc9ddfbcull}},
        {2, 2, {0x97ae38b22d60ad89ull, 0x63a0feb63aa5cdfbull, 0x374cba8358ffbe23ull}},
        {3, 3, {0x1d98b106fd1a223aull, 0xd4b4bdf2b0154909ull, 0x70914135f77e0b37ull}},
        {4, 3, {0xadc509a57a1444f5ull, 0x28c3b1df182093e2ull, 0x23f40e9e08d0664cull}},
        {5, 4, {0x8638a018ee2f6dfeull, 0x0765d5b62fceba0bull, 0xce911c61e7843529ull}},
        {6, 4, {0x38958e551d7f4b26ull, 0x410fd355634e2e9full, 0xbdc858ff3d19be35ull}},
        {7, 4, {0x6e368df6df89772dull, 0x943b4535b5641da8ull, 0xb9a05ff6c336cc4eull}},
    };
    constexpr std::uint32_t seeds[] = {0, 1, 0xdeadbeefu};
    for (const auto& test : cases) {
        for (unsigned seed_index = 0; seed_index < 3; ++seed_index) {
            reset();
            composite_texture.levels = 3;
            composite_texture.pitch_pixels = 8;
            std::uint32_t state = seeds[seed_index];
            std::vector<std::uint32_t> pixels;
            for (UINT index = 0; index < test.width * test.height; ++index) {
                state = state * 1664525u + 1013904223u;
                pixels.push_back(state);
            }
            check(upload_emissive_mip_chain(
                      reinterpret_cast<IDirect3DTexture8*>(&composite_texture),
                      pixels, test.width, test.height), "matrix mip upload");
            std::uint64_t hash = 14695981039346656037ull;
            UINT width = test.width, height = test.height;
            for (UINT level = 0; level < 3; ++level) {
                for (UINT y = 0; y < height; ++y) {
                    for (UINT x = 0; x < width; ++x) {
                        hash ^= composite_texture.rows[level][y * 8 + x];
                        hash *= 1099511628211ull;
                    }
                }
                width = std::max<UINT>(1, width / 2);
                height = std::max<UINT>(1, height / 2);
            }
            check(hash == test.hash[seed_index], "matrix raw-bit hash");
        }
    }
    std::puts("mip-raw-bit-matrix-27:PASS");
}

void test_sources() {
    reset();
    source_texture.width = 3;
    source_texture.height = 1;
    source_texture.pitch_pixels = 4;
    source_texture.rows[0][0] = 0xff000000;
    source_texture.rows[0][1] = 0x00123456;
    source_texture.rows[0][2] = 0;
    EmissiveMaterialPolicy sparse;
    sparse.paths[0] = "sparse";
    check(load_emissive_source(sparse, 0,
              reinterpret_cast<IDirect3DDevice8*>(&device)), "load sparse");
    check(sparse.width == 3 && sparse.height == 1, "source dimensions");
    check(sparse.pixels[0].dense.empty(), "sparse no dense");
    check(sparse.pixels[0].sparse.size() == 1 &&
          sparse.pixels[0].sparse[0].index == 1 &&
          sparse.pixels[0].sparse[0].pixel == 0x00123456,
          "sparse exact index and pixel");
    check(source_texture.unlocks == 1 && source_texture.releases == 1,
          "source unlock and release");
    check(load_emissive_source(sparse, 0,
              reinterpret_cast<IDirect3DDevice8*>(&device)) && loader_calls == 1,
          "source attempted cache");
    std::puts("load-sparse-pitched:PASS");

    reset();
    source_texture.width = 2;
    source_texture.rows[0][0] = 0x00ffffff;
    source_texture.rows[0][1] = 0xff000000;
    EmissiveMaterialPolicy dense;
    dense.paths[0] = "dense";
    check(load_emissive_source(dense, 0,
              reinterpret_cast<IDirect3DDevice8*>(&device)), "load dense tie");
    check(dense.pixels[0].sparse.empty() && dense.pixels[0].dense.size() == 2 &&
          dense.pixels[0].dense[0] == 0x00ffffff &&
          dense.pixels[0].dense[1] == 0xff000000,
          "dense tie and alpha");
    std::puts("load-dense-tie:PASS");

    reset();
    source_texture.width = 3;
    EmissiveMaterialPolicy black;
    black.paths[0] = "black";
    check(load_emissive_source(black, 0,
              reinterpret_cast<IDirect3DDevice8*>(&device)), "load black");
    check(black.pixels[0].dense.empty() && black.pixels[0].sparse.empty(),
          "black sparse empty");
    std::puts("load-black:PASS");

    for (int hook : {1, 2, 3}) {
        reset();
        source_texture.width = 5;
        source_texture.pitch_pixels = 8;
        source_texture.rows[0][0] = 0x00112233;
        sparse_mutation_pixel = &source_texture.rows[0][4];
        EmissiveMaterialPolicy changing;
        changing.paths[0] = "changing";
        sparse_allocation_hook = hook;
        const bool loaded = load_emissive_source(
            changing, 0, reinterpret_cast<IDirect3DDevice8*>(&device));
        check(sparse_allocation_hook == 0, "sparse allocation hook fired");
        check(source_texture.unlocks == 1 && source_texture.releases == 1,
              "sparse allocation cleanup");
        if (hook == 1) {
            check(loaded && changing.source_loaded[0] &&
                  changing.pixels[0].sparse.size() == 2 &&
                  changing.pixels[0].sparse[0].index == 0 &&
                  changing.pixels[0].sparse[1].index == 4 &&
                  changing.pixels[0].sparse[1].pixel == 0x00abcdefu,
                  "second-pass sparse growth retained");
        } else {
            check(!loaded && !changing.source_loaded[0] &&
                  changing.pixels[0].sparse.empty() &&
                  changing.pixels[0].dense.empty(),
                  "sparse allocation failure clears pixels");
        }
    }
    std::puts("load-sparse-mutation-growth-failure:PASS");

    for (int failure : {0, 1, 2}) {
        reset();
        EmissiveMaterialPolicy policy;
        policy.paths[0] = "failure";
        loader_fails = failure == 0;
        source_texture.fail_lock = failure == 1 ? 0 : -1;
        source_texture.null_bits = failure == 2;
        check(!load_emissive_source(policy, 0,
                  reinterpret_cast<IDirect3DDevice8*>(&device)),
              "load failure result");
        check(policy.source_attempted[0] && !policy.source_loaded[0],
              "load failure attempted state");
        check(source_texture.unlocks == 0, "load failure unlock state");
    }
    std::puts("load-failures:PASS");
}

void test_composite() {
    reset();
    composite_texture.pitch_pixels = 4;
    EmissiveMaterialPolicy policy;
    policy.width = 3;
    policy.height = 1;
    policy.path_mask = 3;
    policy.paths[0] = "dense";
    policy.paths[1] = "sparse";
    policy.source_attempted[0] = policy.source_attempted[1] = true;
    policy.source_loaded[0] = policy.source_loaded[1] = true;
    policy.pixels[0].dense = {0x00100000, 0x00000010, 0x00000000};
    policy.pixels[1].sparse = {{1, 0x00002000}, {9, 0xffffffff},
                               {1, 0x00004000}};
    auto* result = build_emissive_composite(
        policy, 3, a2fo::nebula::CraftMotionLightState::impulse,
        reinterpret_cast<IDirect3DDevice8*>(&device));
    check(result == reinterpret_cast<IDirect3DTexture8*>(&composite_texture),
          "composite returned texture");
    const auto intensity = a2fo::nebula::emissive_intensity_percent(
        a2fo::nebula::CraftMotionLightState::impulse);
    auto expected0 = a2fo::nebula::merge_emissive_pixel(
        0xff000000, policy.pixels[0].dense[0], intensity[0]);
    auto expected1 = a2fo::nebula::merge_emissive_pixel(
        0xff000000, policy.pixels[0].dense[1], intensity[0]);
    expected1 = a2fo::nebula::merge_emissive_pixel(
        expected1, policy.pixels[1].sparse[0].pixel, intensity[1]);
    expected1 = a2fo::nebula::merge_emissive_pixel(
        expected1, policy.pixels[1].sparse[2].pixel, intensity[1]);
    check(composite_texture.rows[0][0] == expected0 &&
          composite_texture.rows[0][1] == expected1 &&
          composite_texture.rows[0][2] == 0xff000000,
          "composite dense sparse order percent invalid index");
    check(policy.composites.size() == 1 && create_calls == 1,
          "composite inserted cache");
    check(build_emissive_composite(
              policy, 3, a2fo::nebula::CraftMotionLightState::impulse,
              reinterpret_cast<IDirect3DDevice8*>(&device)) == result &&
          create_calls == 1, "composite cache hit");
    std::puts("composite-order-cache:PASS");

    reset();
    create_target = &black_texture;
    EmissiveMaterialPolicy unavailable;
    unavailable.paths[0] = "missing";
    unavailable.path_mask = 1;
    loader_fails = true;
    check(build_emissive_composite(
              unavailable, 1, a2fo::nebula::CraftMotionLightState::idle,
              reinterpret_cast<IDirect3DDevice8*>(&device)) ==
              reinterpret_cast<IDirect3DTexture8*>(&black_texture),
          "composite loader failure fallback");
    check(black_texture.rows[0][0] == 0xff000000 &&
          black_texture.unlocks == 1 && loader_calls == 1,
          "composite fallback black pixel and cleanup");
    std::puts("composite-loader-fallback:PASS");
}

} // namespace

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    // D3D8 texture indices come from the MinGW d3d8.h interface declaration.
    texture_vtable[2] = reinterpret_cast<void*>(&fake_release);
    texture_vtable[13] = reinterpret_cast<void*>(&fake_level_count);
    texture_vtable[14] = reinterpret_cast<void*>(&fake_level_desc);
    texture_vtable[16] = reinterpret_cast<void*>(&fake_lock);
    texture_vtable[17] = reinterpret_cast<void*>(&fake_unlock);
    device_vtable[20] = reinterpret_cast<void*>(&fake_create);
    test_mips();
    test_mip_raw_bit_matrix();
    test_sources();
    test_composite();
    std::puts("core pixels native fixture: PASS");
    return 0;
}
