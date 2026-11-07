// Headless inventory check: typed D3D11 allocations only, no pixel or quality claims.
#include "../renderer/d3d11/compressed_texture.hpp"

#include <d3d11.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
template<class T> struct Owned {
    T* value = nullptr;
    ~Owned() { if (value) value->Release(); }
    T** out() { return &value; }
    T* operator->() const { return value; }
};

struct Module {
    HMODULE value = LoadLibraryA("d3d11.dll");
    ~Module() { if (value) FreeLibrary(value); }
};

struct Totals {
    std::uint64_t count = 0;
    std::uint64_t compressed = 0;
    std::uint64_t rgba8 = 0;
};

bool absolute_path(const char* path) {
    if (!path || !*path) return false;
    const bool drive = std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
                       (path[2] == '\\' || path[2] == '/');
    const bool unc = path[0] == '\\' && path[1] == '\\';
    return drive || unc || path[0] == '/';
}

bool dds_name(const char* name) {
    const std::string text(name);
    if (text.size() < 5) return false;
    const auto suffix = text.substr(text.size() - 4);
    return std::tolower(static_cast<unsigned char>(suffix[0])) == '.' &&
           std::tolower(static_cast<unsigned char>(suffix[1])) == 'd' &&
           std::tolower(static_cast<unsigned char>(suffix[2])) == 'd' &&
           std::tolower(static_cast<unsigned char>(suffix[3])) == 's';
}

bool names_in_directory(const std::string& directory, std::vector<std::string>& names) {
    const DWORD attributes = GetFileAttributesA(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    const std::string pattern = directory + "\\*.dds";
    WIN32_FIND_DATAA data{};
    HANDLE handle = FindFirstFileA(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) return false;
    do {
        if (!(data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
            dds_name(data.cFileName)) names.emplace_back(data.cFileName);
    } while (FindNextFileA(handle, &data));
    const DWORD status = GetLastError();
    FindClose(handle);
    std::sort(names.begin(), names.end());
    return status == ERROR_NO_MORE_FILES && !names.empty();
}

bool read_bounded(const std::string& path, std::vector<std::uint8_t>& bytes) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    constexpr long limit = 64L * 1024L * 1024L;
    bool good = std::fseek(file, 0, SEEK_END) == 0;
    const long length = good ? std::ftell(file) : -1;
    good = good && length > 0 && length <= limit && std::fseek(file, 0, SEEK_SET) == 0;
    if (good) {
        bytes.resize(static_cast<std::size_t>(length));
        good = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    }
    if (std::fclose(file) != 0) good = false;
    return good;
}

int fail(const std::string& name, const char* operation, HRESULT hr, const std::string& detail = {}) {
    std::fprintf(stderr, "FAIL: %s %s hr=0x%08lx %s\n", name.c_str(), operation,
                 static_cast<unsigned long>(hr), detail.c_str());
    return 1;
}

int inspect(const std::string& directory, Totals& totals) {
    std::vector<std::string> names;
    if (!names_in_directory(directory, names)) {
        std::fprintf(stderr, "FAIL: input directory has no eligible top-level DDS files: %s\n",
                     directory.c_str());
        return 1;
    }
    Module module; // Destroyed after every COM object in the scope below.
    if (!module.value) return fail("d3d11.dll", "LoadLibraryA", HRESULT_FROM_WIN32(GetLastError()));
    {
        using Create = HRESULT (WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
            const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*,
            ID3D11DeviceContext**);
        const auto address = GetProcAddress(module.value, "D3D11CreateDevice");
        Create create = nullptr;
        static_assert(sizeof(create) == sizeof(address));
        std::memcpy(&create, &address, sizeof(create));
        if (!create) return fail("d3d11.dll", "GetProcAddress", HRESULT_FROM_WIN32(GetLastError()));
        Owned<ID3D11Device> device;
        Owned<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL level{};
        const D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
        const HRESULT device_hr = create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &requested, 1, D3D11_SDK_VERSION, device.out(), &level, context.out());
        if (FAILED(device_hr)) {
            std::printf("SKIP: D3D11 hardware device hr=0x%08lx\n",
                        static_cast<unsigned long>(device_hr));
            return 77;
        }
        if (level < requested) return fail("D3D11", "feature level 11", E_FAIL);
        Owned<IDXGIDevice> dxgi_device;
        HRESULT hr = device->QueryInterface(__uuidof(IDXGIDevice),
                                            reinterpret_cast<void**>(dxgi_device.out()));
        if (FAILED(hr)) return fail("adapter", "QueryInterface", hr);
        Owned<IDXGIAdapter> adapter;
        hr = dxgi_device->GetAdapter(adapter.out());
        if (FAILED(hr)) return fail("adapter", "GetAdapter", hr);
        DXGI_ADAPTER_DESC adapter_desc{};
        hr = adapter->GetDesc(&adapter_desc);
        if (FAILED(hr)) return fail("adapter", "GetDesc", hr);
        char description[512]{};
        if (!WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1,
                                 description, sizeof(description), nullptr, nullptr))
            std::snprintf(description, sizeof(description), "(description unavailable)");
        std::printf("ADAPTER vendor=0x%04x device=0x%04x description=%s\n",
                    adapter_desc.VendorId, adapter_desc.DeviceId, description);

        for (const auto& name : names) {
            const std::string path = directory + "\\" + name;
            std::vector<std::uint8_t> bytes;
            if (!read_bounded(path, bytes)) return fail(name, "read DDS (max 64 MiB)", E_FAIL);
            a2fo::renderer::textures::CompressedImage image;
            std::string error;
            hr = a2fo::renderer::textures::parse_compressed_dds(bytes.data(), bytes.size(), image, error);
            if (FAILED(hr)) return fail(name, "parse_compressed_dds", hr, error);
            if (image.format != DXGI_FORMAT_BC5_UNORM && image.format != DXGI_FORMAT_BC5_SNORM &&
                image.format != DXGI_FORMAT_BC7_UNORM && image.format != DXGI_FORMAT_BC7_UNORM_SRGB)
                return fail(name, "expected BC5/BC7", E_INVALIDARG);
            Owned<ID3D11ShaderResourceView> view;
            a2fo::renderer::CompressedTextureInfo info{};
            hr = a2fo::renderer::create_compressed_texture(device.value, image, view.out(), info, error);
            if (FAILED(hr)) return fail(name, "create_compressed_texture", hr, error);
            Owned<ID3D11Resource> resource;
            view->GetResource(resource.out());
            if (!resource.value) return fail(name, "SRV GetResource", E_FAIL);
            Owned<ID3D11Texture2D> texture;
            hr = resource->QueryInterface(IID_ID3D11Texture2D,
                                          reinterpret_cast<void**>(texture.out()));
            if (FAILED(hr)) return fail(name, "resource Texture2D QueryInterface", hr);
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            if (desc.Format != image.format || desc.Width != image.width ||
                desc.Height != image.height || desc.MipLevels != image.mips.size() ||
                desc.ArraySize != 1 || desc.Usage != D3D11_USAGE_IMMUTABLE ||
                desc.CPUAccessFlags != 0 || desc.BindFlags != D3D11_BIND_SHADER_RESOURCE)
                return fail(name, "typed immutable Texture2D descriptor", E_FAIL);
            D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
            view->GetDesc(&srv);
            if (srv.Format != image.format || srv.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D ||
                srv.Texture2D.MostDetailedMip != 0 || srv.Texture2D.MipLevels != image.mips.size())
                return fail(name, "all-mip SRV descriptor", E_FAIL);
            if (info.format != image.format || info.width != image.width ||
                info.height != image.height || info.mip_levels != image.mips.size() ||
                info.block_payload_bytes != image.bytes.size())
                return fail(name, "upload metadata", E_FAIL);
            totals.count++;
            totals.compressed += info.block_payload_bytes;
            totals.rgba8 += info.rgba8_reference_bytes;
        } // View, texture, resource, and encoded bytes released per file.
    } // Device and context released before FreeLibrary.
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3 || !absolute_path(argv[2])) {
        std::fprintf(stderr, "usage: renderer_bc_inventory.exe OUTPUT_DIR ABSOLUTE_INPUT_DIR\n");
        return 2;
    }
    (void)argv[1];
    Totals totals;
    const int result = inspect(argv[2], totals);
    if (result == 0)
        std::printf("PASS: all BC DDS GPU resources count=%llu compressed_payload_bytes=%llu "
                    "rgba8_reference_bytes=%llu (typed allocation only)\n",
                    static_cast<unsigned long long>(totals.count),
                    static_cast<unsigned long long>(totals.compressed),
                    static_cast<unsigned long long>(totals.rgba8));
    return result;
}
