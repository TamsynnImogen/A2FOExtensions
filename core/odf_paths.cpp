/* Normalized recursive ODF path and basename-alias helpers. */
#include "odf_paths.hpp"
#include "../rust/config_ffi.h"
#include <exception>
namespace a2fo {
namespace {
A2foRsBytes span(const std::string &s) {
    return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
}
struct Output {
    std::string *value;
    std::exception_ptr error;
};
void receive(void *p, uint32_t, size_t, const uint8_t *data, size_t size) noexcept {
    auto &out = *static_cast<Output *>(p);
    if (out.error)
        return;
    try {
        out.value->assign(reinterpret_cast<const char *>(data), size);
    } catch (...) {
        out.error = std::current_exception();
    }
}
} // namespace
std::string recursive_odf_basename_key(const std::string &requested_path) {
    std::string result;
    Output output{&result, {}};
    a2fo_rs_config_odf_basename(span(requested_path), receive, &output);
    if (output.error)
        std::rethrow_exception(output.error);
    return result;
}
bool find_recursive_odf_alias(const std::string &requested_path,
                              const std::map<std::string, std::string> &aliases,
                              std::string &aliased_path) {
    aliased_path.clear();
    const std::string basename = recursive_odf_basename_key(requested_path);
    if (basename.empty())
        return false;
    const auto found = aliases.find(basename);
    if (found == aliases.end() || found->second.empty() ||
        a2fo_rs_config_odf_alias_equivalent(span(found->second), span(requested_path)))
        return false;
    aliased_path = found->second;
    return true;
}
} // namespace a2fo
