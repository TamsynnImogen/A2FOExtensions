#include "additional_resources.hpp"
#include "api.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using namespace a2fo::resources;

    static_assert(A2FO_RESOURCE_TRITANIUM == 6);
    static_assert(A2FO_RESOURCE_SUPPLY == 7);
    static_assert(A2FO_RESOURCE_CREDITS == 8);
    static_assert(A2FO_RESOURCE_COLLECTIVE_CONNECTIONS == 9);
    static_assert(A2FO_RESOURCE_COUNT == 10);
    static_assert(A2FO_RESOURCE_PRESENTATION_RES == 0);
    static_assert(A2FO_RESOURCE_PRESENTATION_TOOLTIP == 1);
    static_assert(A2FO_RESOURCE_PRESENTATION_VERBOSE_TOOLTIP == 2);
    static_assert(A2FO_RESOURCE_PRESENTATION_ICON == 3);
    static_assert(A2FO_RESOURCE_PRESENTATION_COUNT == 4);

    std::int32_t value = -1;
    assert(parse_nonnegative(" 42 ", &value) && value == 42);
    assert(parse_nonnegative("0", &value) && value == 0);
    assert(!parse_nonnegative("-1", &value));
    assert(!parse_nonnegative("12foo", &value));
    assert(!parse_nonnegative("", &value));

    Amounts amounts{{100, 20, 30, 4}};
    Costs costs{{40, 5, 30, 4}};
    assert(can_afford(amounts, costs));
    debit(amounts, costs);
    assert((amounts == Amounts{{60, 15, 0, 0}}));
    assert(!can_afford(amounts, costs));
    credit(amounts, costs);
    assert((amounts == Amounts{{100, 20, 30, 4}}));

    assert(saturating_add(std::numeric_limits<std::int64_t>::max(), 1) ==
           std::numeric_limits<std::int64_t>::max());
    assert(saturating_add(std::numeric_limits<std::int64_t>::min(), -1) ==
           std::numeric_limits<std::int64_t>::min());
    return 0;
}
