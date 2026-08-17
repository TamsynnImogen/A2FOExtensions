#include "additional_resources.hpp"

#include <charconv>
#include <limits>

namespace a2fo::resources {

bool parse_nonnegative(std::string_view text, std::int32_t* value) noexcept {
    if (!value) return false;
    while (!text.empty() &&
           (text.front() == ' ' || text.front() == '\t' ||
            text.front() == '\r' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() &&
           (text.back() == ' ' || text.back() == '\t' ||
            text.back() == '\r' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    if (text.empty()) return false;
    std::int32_t parsed = 0;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec != std::errc{} ||
        result.ptr != text.data() + text.size() || parsed < 0) {
        return false;
    }
    *value = parsed;
    return true;
}

bool can_afford(const Amounts& amounts, const Costs& costs) noexcept {
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        if (amounts[index] < static_cast<std::int64_t>(costs[index])) {
            return false;
        }
    }
    return true;
}

void debit(Amounts& amounts, const Costs& costs) noexcept {
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        amounts[index] -= static_cast<std::int64_t>(costs[index]);
    }
}

std::int64_t saturating_add(std::int64_t value,
                            std::int64_t delta) noexcept {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    if (delta > 0 && value > maximum - delta) return maximum;
    if (delta < 0 && value < minimum - delta) return minimum;
    return value + delta;
}

void credit(Amounts& amounts, const Costs& costs) noexcept {
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        amounts[index] = saturating_add(
            amounts[index], static_cast<std::int64_t>(costs[index]));
    }
}

}  // namespace a2fo::resources
