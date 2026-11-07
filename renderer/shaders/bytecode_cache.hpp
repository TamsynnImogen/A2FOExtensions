#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::renderer {

struct ShaderCompileRequest {
    std::string source;
    std::string entry;
    std::string target;
    UINT flags = 0;
};

struct ShaderCompileResult {
    std::vector<std::uint8_t> bytes;
    bool cache_hit = false;
    bool cache_written = false;
    unsigned long long compile_us = 0;
    std::string diagnostics;
};

// An empty cache_directory compiles without disk I/O. Cache failures do not
// change the result of a valid compilation.
HRESULT compile_shader(const ShaderCompileRequest& request,
                       const std::string& cache_directory,
                       ShaderCompileResult& result);

} // namespace a2fo::renderer
