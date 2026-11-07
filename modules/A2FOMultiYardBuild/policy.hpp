#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo::multi_yard {

// Concept adapted from armada2-refit qol/qol.c QOL6, MIT, pinned at
// 37d60079169b086dba4216fe55e05cd6015d66c0. This policy is independent.
struct Candidate {
    std::uint32_t handle;
    std::uint32_t queued;
};

inline constexpr std::size_t npos = SIZE_MAX;

inline bool valid_candidates(const Candidate* rows, std::size_t count) {
    if (!rows || count == 0 || count > 30) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (rows[i].handle == 0) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (rows[j].handle == rows[i].handle) return false;
        }
    }
    return true;
}

inline std::size_t selection_start(const Candidate* rows, std::size_t count,
                                   std::uint32_t last_handle) {
    for (std::size_t i = 0; i < count; ++i) {
        if (rows[i].handle == last_handle) return (i + 1) % count;
    }
    return 0;
}

inline std::size_t choose_build(const Candidate* rows, std::size_t count,
                                std::uint32_t last_handle) {
    if (!valid_candidates(rows, count)) return npos;
    const std::size_t start = selection_start(rows, count, last_handle);
    std::size_t best = start;
    for (std::size_t offset = 1; offset < count; ++offset) {
        const std::size_t index = (start + offset) % count;
        if (rows[index].queued < rows[best].queued) best = index;
    }
    return best;
}

inline std::size_t choose_cancel(const Candidate* rows, std::size_t count) {
    if (!valid_candidates(rows, count)) return npos;
    std::size_t best = 0;
    for (std::size_t index = 1; index < count; ++index) {
        if (rows[index].queued > rows[best].queued) best = index;
    }
    return best;
}

} // namespace a2fo::multi_yard
