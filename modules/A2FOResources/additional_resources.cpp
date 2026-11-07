#include "additional_resources.hpp"
#include "../../rust/gameplay_ffi.h"

namespace a2fo::resources {

bool parse_nonnegative(std::string_view text, std::int32_t *value) noexcept {
    return a2fo_rs_gameplay_parse_nonnegative(reinterpret_cast<const std::uint8_t *>(text.data()),
                                              text.size(), value) != 0;
}

bool can_afford(const Amounts &amounts, const Costs &costs) noexcept {
    return a2fo_rs_gameplay_can_afford(amounts.data(), costs.data()) != 0;
}

void debit(Amounts &amounts, const Costs &costs) noexcept {
    a2fo_rs_gameplay_debit(amounts.data(), costs.data());
}

std::int64_t saturating_add(std::int64_t value, std::int64_t delta) noexcept {
    return a2fo_rs_gameplay_saturating_add(value, delta);
}

void credit(Amounts &amounts, const Costs &costs) noexcept {
    a2fo_rs_gameplay_credit(amounts.data(), costs.data());
}

} // namespace a2fo::resources
