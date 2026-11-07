#include "../core/initialize_boundary.hpp"
#include <cassert>
#include <new>
#include <stdexcept>
int main() {
    unsigned recovered=0;
    const auto recover=[&]() noexcept { ++recovered; };
    const auto initialize=[]{return true;};
    static_assert(noexcept(a2fo::initialize_without_unwind(initialize,recover)));
    assert(a2fo::initialize_without_unwind([]{return true;},recover));
    assert(!a2fo::initialize_without_unwind([]{return false;},recover));
    assert(recovered==0);
    assert(!a2fo::initialize_without_unwind([]()->bool{throw std::bad_alloc{};},recover));
    assert(!a2fo::initialize_without_unwind([]()->bool{throw std::runtime_error("native failure");},recover));
    assert(!a2fo::initialize_without_unwind([]()->bool{throw 7;},recover));
    assert(recovered==3);
}
