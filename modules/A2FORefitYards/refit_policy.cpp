#include "refit_policy.hpp"
#include "../../rust/config_ffi.h"
#include <exception>
namespace a2fo::refit {
namespace {
A2foRsBytes span(const std::string &s) {
    return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
}
struct StringOutput {
    std::string *value;
    std::exception_ptr error;
};
void receive_string(void *p, uint32_t, size_t, const uint8_t *data, size_t size) noexcept {
    auto &out = *static_cast<StringOutput *>(p);
    if (out.error)
        return;
    try {
        out.value->assign(reinterpret_cast<const char *>(data), size);
    } catch (...) {
        out.error = std::current_exception();
    }
}
struct PolicyOutput {
    ClassPolicy *policy;
    std::exception_ptr error;
};
void receive_policy(void *p, uint32_t kind, size_t, const uint8_t *data, size_t size) noexcept {
    auto &out = *static_cast<PolicyOutput *>(p);
    if (out.error)
        return;
    try {
        std::string value(reinterpret_cast<const char *>(data), size);
        switch (kind) {
        case 0:
            out.policy->refit_items.push_back(std::move(value));
            break;
        case 1:
            out.policy->refit_hardpoint = std::move(value);
            break;
        case 2:
            out.policy->build_hardpoint = std::move(value);
            break;
        case 3:
            out.policy->class_label = std::move(value);
            break;
        }
    } catch (...) {
        out.error = std::current_exception();
    }
}
} // namespace
std::string normalize_odf_value(std::string value) {
    std::string result;
    StringOutput out{&result, {}};
    a2fo_rs_config_refit_normalize(span(value), receive_string, &out);
    if (out.error)
        std::rethrow_exception(out.error);
    return result;
}
ClassPolicy parse_class_policy(const OdfFields &fields) {
    std::vector<A2foRsBytes> keys, values;
    keys.reserve(fields.size());
    values.reserve(fields.size());
    for (const auto &[key, value] : fields) {
        keys.push_back(span(key));
        values.push_back(span(value));
    }
    ClassPolicy result;
    PolicyOutput out{&result, {}};
    a2fo_rs_config_refit_parse(keys.data(), values.data(), fields.size(), receive_policy, &out);
    if (out.error)
        std::rethrow_exception(out.error);
    return result;
}
bool ClassPolicy::is_supported_yard() const noexcept {
    return a2fo_rs_config_refit_supported(span(refit_hardpoint), span(build_hardpoint),
                                          span(class_label)) != 0;
}
} // namespace a2fo::refit
