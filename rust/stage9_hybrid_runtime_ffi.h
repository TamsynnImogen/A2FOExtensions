#ifndef A2FO_STAGE9_HYBRID_RUNTIME_FFI_H
#define A2FO_STAGE9_HYBRID_RUNTIME_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {
using A2foHybridStage9Step = std::uint64_t (*)(void*, std::uint32_t,
                                               std::size_t);
std::uint64_t a2fo_rs_stage9_hybrid_runtime(
    std::uint32_t kind, void* context, A2foHybridStage9Step step);
}

#endif
