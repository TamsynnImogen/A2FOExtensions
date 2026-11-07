#include "../modules/A2FOCollectiveXP/queue_resolver.hpp"

#include <cstdio>
#include <cstdlib>

using a2fo::collective_xp::resolve_queue_class;

namespace {
void* expected_producer;
void* expected_base;
void* replacement;
bool called;

void* resolve(void* producer, void* base) {
    called = true;
    if (producer != expected_producer || base != expected_base) {
        std::fputs("queue resolver received wrong producer or base\n", stderr);
        std::exit(1);
    }
    return replacement;
}

void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "collective_xp_queue_test: %s\n", message);
        std::exit(1);
    }
}
} // namespace

int main() {
    int producer = 0, base = 0, ranked = 0;
    expected_producer = &producer;
    expected_base = &base;
    check(resolve_queue_class(&producer, &base, nullptr) == &base,
          "missing resolver preserves base class");
    called = false;
    replacement = &ranked;
    check(resolve_queue_class(&producer, &base, resolve) == &ranked && called,
          "resolver receives producer and base and returns replacement");
    called = false;
    replacement = nullptr;
    check(resolve_queue_class(&producer, &base, resolve) == nullptr && called,
          "null resolver result rejects admission");
    std::puts("collective_xp_queue_test: ok");
}
