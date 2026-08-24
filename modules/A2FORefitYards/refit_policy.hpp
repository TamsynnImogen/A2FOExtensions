/* Pure ODF policy helpers for A2FORefitYards. */

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace a2fo::refit {

using OdfFields = std::vector<std::pair<std::string, std::string>>;

struct ClassPolicy {
    std::vector<std::string> refit_items;
    std::string refit_hardpoint;
    std::string build_hardpoint;
    std::string class_label;

    bool is_refit_source() const noexcept {
        return !refit_items.empty();
    }
    bool is_supported_yard() const noexcept;
};

std::string normalize_odf_value(std::string value);
ClassPolicy parse_class_policy(const OdfFields& fields);

}  // namespace a2fo::refit
