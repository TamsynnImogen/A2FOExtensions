#include "../core/runtime_handlers.hpp"
#include <cassert>
#include <string>
#include <vector>

struct FieldRegistration {
    std::string owner;
    std::vector<std::string> required_odf_fields;
    void (*handler)(void*) = nullptr;
    void* user_data = nullptr;
    bool enabled = true;
};
struct EventRegistration {
    std::string owner;
    void (*handler)(void*) = nullptr;
    void* user_data = nullptr;
};
static void callback(void*) {}

int main() {
    a2fo::RustHandlerRegistry<FieldRegistration> handlers;
    const std::string owner("a\0\xff", 3);
    const std::string field("\xff\0z", 3);
    handlers.push_back({owner, {field, field}, callback, reinterpret_cast<void*>(0x1234), true});
    auto snapshot = handlers;
    assert(snapshot.size() == 1);
    const auto copy = snapshot.get(0);
    assert(copy.owner == owner && copy.required_odf_fields == std::vector<std::string>({field, field}));
    assert(copy.handler == callback && copy.user_data == reinterpret_cast<void*>(0x1234) && copy.enabled);
    assert(std::string(snapshot.cbegin()->owner.data, snapshot.cbegin()->owner.size) == owner);
    auto leased = handlers.borrow(0);
    auto retained = leased;
    assert(std::string(leased.owner.data, leased.owner.size) == owner);
    handlers.set_enabled(0, false);
    handlers.set_handler(0, nullptr);
    assert(handlers.get(0).handler == nullptr && snapshot.get(0).handler == callback);
    assert(!handlers.get(0).enabled && snapshot.get(0).enabled);
    handlers.clear();
    assert(handlers.empty() && snapshot.size() == 1);
    assert(leased.handler == callback && retained.handler == callback);
    assert(std::string(retained.owner.data, retained.owner.size) == owner);
    auto iterator = snapshot.cbegin();
    assert(iterator->handler == callback);
    snapshot.clear();
    assert(iterator->handler == callback);
    a2fo::RustHandlerRegistry<EventRegistration> events;
    events.push_back({owner, callback, nullptr});
    assert(events[0].owner == owner && events[0].handler == callback);
}
