// Actual Resources lookup/payment/refund paths with a cached squad provider.
#include "../modules/A2FOResources/module.cpp"
#include <cassert>
#include <iostream>

namespace {
int squad_class, ordinary_class, team;
const Costs squad_price{{31, 38, 49, 56}};
bool A2FO_CALL squad_provider(void* klass, std::int32_t* costs, std::uint32_t count) {
    if (klass != &squad_class || count != 4) return false;
    std::copy(squad_price.begin(), squad_price.end(), costs);
    return true;
}
}

int main() {
    InitializeCriticalSection(&g_lock);
    g_lock_ready = g_runtime_alive = g_production_integration_ready = true;
    const Costs ordinary{{1, 2, 3, 4}};
    g_class_costs[&ordinary_class] = ordinary;
    g_class_costs[&squad_class] = {{999, 999, 999, 999}};
    // Ordinary Resources remains functional before the optional module loads.
    assert(class_costs(&ordinary_class) == ordinary);
    g_squad_costs = &squad_provider;
    assert(class_costs(&ordinary_class) == ordinary);
    assert(class_costs(&squad_class) == squad_price);
    for (unsigned i = 0; i < 4; ++i)
        assert(A2FOResources_GetCost(&squad_class, i + 6) == squad_price[i]);
    std::array<std::uint8_t, 0x100> producer{};
    void* team_pointer = &team;
    std::memcpy(producer.data() + kProducerTeamOffset, &team_pointer, sizeof(team_pointer));
    const Amounts initial{{100, 100, 100, 100}};
    g_team_amounts[&team] = initial;
    assert(a2fo_resources_can_pay(producer.data(), &squad_class));
    a2fo_resources_commit_payment(producer.data(), &squad_class);
    const Amounts paid{{69, 62, 51, 44}};
    assert(g_team_amounts[&team] == paid);
    assert(!a2fo_resources_can_pay(producer.data(), &squad_class));
    a2fo_resources_commit_payment(producer.data(), &squad_class);
    assert(g_team_amounts[&team] == paid); // No partial second payment.
    A2FO_ProducerEvent event{};
    event.struct_size = sizeof(event);
    event.producer = producer.data();
    event.target_class = &squad_class;
    for (auto kind : {A2FO_PRODUCER_EVENT_CANCELLED, A2FO_PRODUCER_EVENT_DELETED,
                     A2FO_PRODUCER_EVENT_CLEARED}) {
        g_team_amounts[&team] = paid;
        event.kind = kind;
        assert(producer_event_handler(&event, nullptr));
        assert(g_team_amounts[&team] == initial);
    }
    DeleteCriticalSection(&g_lock);
    std::cout << "PASS squad added-resource tooltip costs, affordability, exact debit, cancellation/deletion/clear refunds and ordinary fallback\n";
}
