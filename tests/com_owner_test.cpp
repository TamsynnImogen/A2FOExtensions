#include "../core/com_owner.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

struct FakeComObject {
    std::string name;
    unsigned references = 1;
    std::vector<std::string>* events = nullptr;

    unsigned AddRef() noexcept {
        events->push_back("add " + name);
        return ++references;
    }

    unsigned Release() noexcept {
        events->push_back("release " + name);
        return --references;
    }
};

}  // namespace

int main() {
    std::vector<std::string> events;
    FakeComObject old_device{"old", 2, &events};
    FakeComObject new_device{"new", 1, &events};
    FakeComObject* owner = &old_device;

    const bool switched = a2fo::adopt_com_owner(
        owner, &new_device, [&events](FakeComObject* device) noexcept {
            events.push_back("cleanup " + device->name);
        });
    assert(switched);
    assert(owner == &new_device);
    assert(old_device.references == 1);
    assert(new_device.references == 2);
    assert((events == std::vector<std::string>{
        "add new", "cleanup old", "release old"}));

    events.clear();
    assert(!a2fo::adopt_com_owner(
        owner, &new_device, [&events](FakeComObject*) noexcept {
            events.push_back("unexpected cleanup");
        }));
    assert(events.empty());

    a2fo::release_com_owner(
        owner, [&events](FakeComObject* device) noexcept {
            events.push_back("cleanup " + device->name);
        });
    assert(owner == nullptr);
    assert(new_device.references == 1);
    assert((events == std::vector<std::string>{
        "cleanup new", "release new"}));

    events.clear();
    FakeComObject retained{"retained", 2, &events};
    FakeComObject unrelated{"unrelated", 1, &events};
    owner = &retained;
    assert(!a2fo::release_matching_com_owner(
        owner, &unrelated, [&events](FakeComObject* device) noexcept {
            events.push_back("cleanup " + device->name);
        }));
    assert(owner == &retained);
    assert(events.empty());
    assert(a2fo::release_matching_com_owner(
        owner, &retained, [&events](FakeComObject* device) noexcept {
            events.push_back("cleanup " + device->name);
        }));
    assert(owner == nullptr);
    assert(retained.references == 1);
    assert((events == std::vector<std::string>{
        "cleanup retained", "release retained"}));
    return 0;
}
