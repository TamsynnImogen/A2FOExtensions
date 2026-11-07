#include "resupply_policy.hpp"

#include "../../rust/resupply_ffi.h"

#include <cstdint>
#include <utility>

namespace a2fo::energy_systems {
namespace {

const std::uint8_t* bytes(std::string_view value) noexcept {
    return reinterpret_cast<const std::uint8_t*>(value.data());
}

ResupplyTargetFilter invalid_filter() noexcept {
    ResupplyTargetFilter filter;
    filter.valid = false;
    filter.default_allow = false;
    return filter;
}

std::string normalize(std::string_view raw,
                      std::size_t (*function)(const std::uint8_t*, std::size_t,
                                              std::uint8_t*, std::size_t)) {
    // Neither normalizer can produce more bytes than the input.
    std::string result(raw.size(), '\0');
    const std::size_t length = function(bytes(raw), raw.size(),
        reinterpret_cast<std::uint8_t*>(result.data()), result.size());
    result.resize(length);
    return result;
}

struct ParseContext {
    ResupplyTargetFilter* filter;
};

std::uint8_t receive_rule(void* opaque, std::uint8_t kind,
                          const std::uint8_t* key, std::size_t length,
                          std::uint8_t allow) noexcept {
    try {
        auto* context = static_cast<ParseContext*>(opaque);
        const std::string name(reinterpret_cast<const char*>(key), length);
        if (kind == 0) {
            context->filter->types.emplace(name, allow != 0);
        } else if (kind == 1) {
            context->filter->odfs.emplace(name, allow != 0);
        } else {
            return 0;
        }
        return 1;
    } catch (...) {
        // An exception must never cross a Rust extern "C" frame.
        return 0;
    }
}

std::int8_t lookup(const std::unordered_map<std::string, bool>& rules,
                   const std::string& key) noexcept {
    if (key.empty()) return -1;
    const auto found = rules.find(key);
    return found == rules.end() ? -1 : static_cast<std::int8_t>(found->second);
}

}  // namespace

std::string normalize_resupply_type(std::string_view raw) {
    return normalize(raw, a2fo_rs_resupply_normalize_type);
}

std::string normalize_resupply_odf(std::string_view raw) {
    return normalize(raw, a2fo_rs_resupply_normalize_odf);
}

ResupplyTargetFilter parse_resupply_targets(std::string_view raw) noexcept {
    try {
        ResupplyTargetFilter filter;
        ParseContext context{&filter};
        std::uint8_t default_allow = 0;
        if (!a2fo_rs_resupply_parse(bytes(raw), raw.size(), &context,
                                    receive_rule, &default_allow)) {
            return invalid_filter();
        }
        filter.default_allow = default_allow != 0;
        return filter;
    } catch (...) {
        return invalid_filter();
    }
}

bool resupply_target_allowed(
    const ResupplyTargetFilter& filter,
    const std::string& normalized_type,
    const std::string& normalized_odf,
    bool same_object,
    bool can_resupply_self) noexcept {
    return a2fo_rs_resupply_allowed(filter.valid, filter.default_allow,
                                    lookup(filter.types, normalized_type),
                                    lookup(filter.odfs, normalized_odf),
                                    same_object, can_resupply_self) != 0;
}

}  // namespace a2fo::energy_systems
