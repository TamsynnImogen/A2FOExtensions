#include "refit_policy.hpp"

#include <algorithm>
#include <cctype>

namespace a2fo::refit {
namespace {

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

const std::string* find_field(const OdfFields& fields,
                              const std::string& name) {
    const std::string wanted = lower_ascii(name);
    for (const auto& field : fields) {
        if (lower_ascii(field.first) == wanted) return &field.second;
    }
    return nullptr;
}

}  // namespace

std::string normalize_odf_value(std::string value) {
    const auto space = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };
    value.erase(value.begin(),
                std::find_if_not(value.begin(), value.end(), space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), space).base(),
                value.end());
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

ClassPolicy parse_class_policy(const OdfFields& fields) {
    ClassPolicy policy;
    for (unsigned index = 0; index < 16; ++index) {
        const std::string name = "refitItem" + std::to_string(index);
        const std::string* raw = find_field(fields, name);
        if (!raw) continue;
        std::string item = normalize_odf_value(*raw);
        if (item.empty()) continue;
        const auto duplicate = std::find_if(
            policy.refit_items.begin(), policy.refit_items.end(),
            [&](const std::string& present) {
                return lower_ascii(present) == lower_ascii(item);
            });
        if (duplicate == policy.refit_items.end()) {
            policy.refit_items.push_back(std::move(item));
        }
    }
    if (const std::string* value = find_field(fields, "refitHardpoint")) {
        policy.refit_hardpoint = normalize_odf_value(*value);
    }
    if (const std::string* value = find_field(fields, "buildHardpoint")) {
        policy.build_hardpoint = normalize_odf_value(*value);
    }
    if (const std::string* value = find_field(fields, "classLabel")) {
        policy.class_label = normalize_odf_value(*value);
    }
    return policy;
}

bool ClassPolicy::is_supported_yard() const noexcept {
    return !refit_hardpoint.empty() && !build_hardpoint.empty() &&
        lower_ascii(class_label) == "shipyard" &&
        lower_ascii(refit_hardpoint) == lower_ascii(build_hardpoint);
}

}  // namespace a2fo::refit
