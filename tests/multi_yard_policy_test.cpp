#include "../modules/A2FOMultiYardBuild/policy.hpp"

#include <array>
#include <cassert>
#include <cstdint>

namespace policy = a2fo::multi_yard;

int main() {
    // Repeated build choices distribute five orders over three yards as 2+2+1.
    std::array<policy::Candidate, 3> rows{{{10, 0}, {20, 0}, {30, 0}}};
    std::uint32_t last = 0;
    for (std::size_t expected : {0u, 1u, 2u, 0u, 1u}) {
        const std::size_t chosen = policy::choose_build(rows.data(), rows.size(), last);
        assert(chosen == expected);
        ++rows[chosen].queued;
        last = rows[chosen].handle;
    }
    assert(rows[0].queued == 2 && rows[1].queued == 2 && rows[2].queued == 1);

    const std::array<policy::Candidate, 3> reordered{{{30, 2}, {10, 2}, {20, 2}}};
    assert(policy::choose_build(reordered.data(), reordered.size(), 10) == 2);
    assert(policy::choose_build(reordered.data(), reordered.size(), 20) == 0);
    assert(policy::choose_build(reordered.data(), reordered.size(), 30) == 1);
    assert(policy::choose_build(reordered.data(), reordered.size(), 999) == 0);

    const std::array<policy::Candidate, 3> uneven{{{30, 3}, {10, 1}, {20, 2}}};
    assert(policy::choose_build(uneven.data(), uneven.size(), 10) == 1);
    assert(policy::choose_cancel(uneven.data(), uneven.size()) == 0);

    const std::array<policy::Candidate, 3> full{{{30, UINT32_MAX}, {10, UINT32_MAX}, {20, UINT32_MAX}}};
    assert(policy::choose_build(full.data(), full.size(), 10) == 2);
    assert(policy::choose_cancel(full.data(), full.size()) == 0);

    const std::array<policy::Candidate, 3> running{{{30, 1}, {10, 0}, {20, 1}}};
    assert(policy::choose_cancel(running.data(), running.size()) == 0);
    const policy::Candidate only_running{7, 1};
    assert(policy::choose_cancel(&only_running, 1) == 0);
    const policy::Candidate empty{7, 0};
    assert(policy::choose_cancel(&empty, 1) == 0);

    assert(policy::choose_build(nullptr, 1, 0) == policy::npos);
    assert(policy::choose_cancel(nullptr, 1) == policy::npos);
    assert(policy::choose_build(rows.data(), 0, 0) == policy::npos);
    assert(policy::choose_cancel(rows.data(), 0) == policy::npos);
    const std::array<policy::Candidate, 31> too_many{};
    assert(policy::choose_build(too_many.data(), too_many.size(), 0) == policy::npos);
    assert(policy::choose_cancel(too_many.data(), too_many.size()) == policy::npos);
    const std::array<policy::Candidate, 2> duplicate{{{1, 0}, {1, 1}}};
    assert(policy::choose_build(duplicate.data(), duplicate.size(), 0) == policy::npos);
    assert(policy::choose_cancel(duplicate.data(), duplicate.size()) == policy::npos);
    const policy::Candidate zero_handle{0, 0};
    assert(policy::choose_build(&zero_handle, 1, 0) == policy::npos);
    assert(policy::choose_cancel(&zero_handle, 1) == policy::npos);
}
