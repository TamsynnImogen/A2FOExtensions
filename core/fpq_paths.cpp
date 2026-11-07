/*
 * File: core/fpq_paths.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: FPQ archive metadata and traversal helpers used to discover mod assets inside packed
 * extension containers.
 */
#include "fpq_paths.hpp"
#include "../rust/config_ffi.h"
#include <exception>
namespace a2fo {
namespace {
struct Output {
    FpqPathResult *result;
    std::exception_ptr error;
};
void receive(void *p, uint32_t kind, size_t, const uint8_t *data, size_t size) noexcept {
    auto &out = *static_cast<Output *>(p);
    if (out.error)
        return;
    try {
        if (kind == 0)
            out.result->odf_directories.emplace_back(reinterpret_cast<const char *>(data), size);
        else
            out.result->error.assign(reinterpret_cast<const char *>(data), size);
    } catch (...) {
        out.error = std::current_exception();
    }
}
} // namespace
FpqPathResult parse_fpq_odf_directories(const std::vector<std::uint8_t> &bytes) {
    FpqPathResult result;
    Output out{&result, {}};
    result.ok = a2fo_rs_config_fpq(bytes.data(), bytes.size(), &result.version, &result.file_count,
                                   receive, &out) != 0;
    if (out.error)
        std::rethrow_exception(out.error);
    return result;
}
} // namespace a2fo
