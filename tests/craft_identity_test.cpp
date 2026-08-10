#include "identity_selection.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

int main() {
    using a2fo::craft_identity::aligned_identity_index;

    std::size_t index = 99;
    assert(!aligned_identity_index(-1, 10, &index));
    assert(index == 0);
    assert(!aligned_identity_index(0, 0, &index));
    assert(!aligned_identity_index(10, 10, &index));
    assert(aligned_identity_index(0, 10, &index));
    assert(index == 0);
    assert(aligned_identity_index(7, 10, &index));
    assert(index == 7);
    assert(aligned_identity_index(9, 10, nullptr));
    return 0;
}
