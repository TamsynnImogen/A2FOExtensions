#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace a2fo::resources {

enum class Resource : std::uint32_t {
    Tritanium = 0,
    Supply = 1,
    Credits = 2,
    CollectiveConnections = 3,
    Count = 4,
};

constexpr std::size_t kResourceCount =
    static_cast<std::size_t>(Resource::Count);

using Amounts = std::array<std::int64_t, kResourceCount>;
using Costs = std::array<std::int32_t, kResourceCount>;

bool parse_nonnegative(std::string_view text, std::int32_t* value) noexcept;
bool can_afford(const Amounts& amounts, const Costs& costs) noexcept;
void debit(Amounts& amounts, const Costs& costs) noexcept;
void credit(Amounts& amounts, const Costs& costs) noexcept;
std::int64_t saturating_add(std::int64_t value,
                            std::int64_t delta) noexcept;

}  // namespace a2fo::resources
