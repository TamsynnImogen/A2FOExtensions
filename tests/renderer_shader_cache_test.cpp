#include "../renderer/shaders/bytecode_cache.hpp"

#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using a2fo::renderer::ShaderCompileRequest;
using a2fo::renderer::ShaderCompileResult;
using a2fo::renderer::compile_shader;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

std::string cache_file(const std::string& directory) {
    WIN32_FIND_DATAA entry{};
    HANDLE search = FindFirstFileA((directory + "\\shader-*.bin").c_str(), &entry);
    require(search != INVALID_HANDLE_VALUE, "cache file exists");
    const std::string path = directory + "\\" + entry.cFileName;
    FindClose(search);
    return path;
}

void damage(const std::string& path, bool truncate) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "open cache for damage");
    if (truncate) {
        LARGE_INTEGER start{};
        require(SetFilePointerEx(file, start, nullptr, FILE_BEGIN) && SetEndOfFile(file),
                "truncate cache");
    } else {
        const char invalid = 'X';
        DWORD count = 0;
        require(WriteFile(file, &invalid, 1, &count, nullptr) && count == 1,
                "corrupt cache header");
    }
    CloseHandle(file);
}

} // namespace

int main() {
    char temp[MAX_PATH]{};
    const DWORD length = GetTempPathA(sizeof(temp), temp);
    require(length && length < sizeof(temp), "temporary root");
    std::string directory;
    for (unsigned i = 0; i < 100; ++i) {
        char suffix[96]{};
        std::snprintf(suffix, sizeof(suffix), "a2fo-shader-%lu-%llu-%u",
                      static_cast<unsigned long>(GetCurrentProcessId()),
                      static_cast<unsigned long long>(GetTickCount64()), i);
        directory = std::string(temp) + suffix;
        if (CreateDirectoryA(directory.c_str(), nullptr)) break;
        require(GetLastError() == ERROR_ALREADY_EXISTS, "create isolated cache directory");
        if (i == 99) require(false, "unique cache directory");
    }

    ShaderCompileRequest request{
        "float4 VS(float4 p : POSITION) : SV_Position { return p; }\n"
        "float4 Other(float4 p : POSITION) : SV_Position { return p * 2; }\n"
        "float4 PS(float4 p : SV_Position) : SV_Target { return float4(1,0,0,1); }\n",
        "VS", "vs_5_0", 0};
    ShaderCompileResult first, second, output;
    const HRESULT initial = compile_shader(request, directory, first);
    if (FAILED(initial)) std::fprintf(stderr, "compile diagnostics: %s\n", first.diagnostics.c_str());
    require(SUCCEEDED(initial) && first.bytes.size() >= 4 &&
            std::memcmp(first.bytes.data(), "DXBC", 4) == 0,
            "initial SM5 compile produces DXBC");
    require(!first.cache_hit && first.cache_written, "first compile writes cache");
    require(SUCCEEDED(compile_shader(request, directory, second)) && second.cache_hit &&
            second.bytes == first.bytes && !second.cache_written,
            "second compile hits byte-identical cache");

    const std::string original_file = cache_file(directory);
    damage(original_file, false);
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written && output.bytes == first.bytes,
            "corrupt cache recompiles and repairs");
    damage(original_file, true);
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written && output.bytes == first.bytes,
            "truncated cache recompiles and repairs");

    request.source += "// source key change\n";
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written, "source invalidates cache");
    request.entry = "Other";
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written, "entry invalidates cache");
    request.target = "vs_4_0";
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written, "target invalidates cache");
    request.entry = "PS";
    request.target = "ps_5_0";
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written, "pixel shader profile compiles");
    request.flags = D3DCOMPILE_ENABLE_STRICTNESS;
    require(SUCCEEDED(compile_shader(request, directory, output)) && !output.cache_hit &&
            output.cache_written, "flags invalidate cache");
    require(SUCCEEDED(compile_shader(request, "", output)) && !output.cache_hit &&
            !output.cache_written && !output.bytes.empty(), "empty directory compiles without cache");
    require(SUCCEEDED(compile_shader(request, original_file, output)) && !output.cache_hit &&
            !output.cache_written && !output.bytes.empty(),
            "cache I/O failure preserves valid compile");

    WIN32_FIND_DATAA entry{};
    HANDLE search = FindFirstFileA((directory + "\\*").c_str(), &entry);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (std::strcmp(entry.cFileName, ".") && std::strcmp(entry.cFileName, ".."))
                DeleteFileA((directory + "\\" + entry.cFileName).c_str());
        } while (FindNextFileA(search, &entry));
        FindClose(search);
    }
    require(RemoveDirectoryA(directory.c_str()), "remove isolated cache directory");
    std::puts("PASS: SM5 compile, cache hit, four key changes, corruption recovery and cache I/O fallback");
    return 0;
}
