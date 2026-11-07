#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
// Synchronous scan of a native chain. 0=count, 1=pointer equality at index.
// Native storage remains in C++; callbacks capture exceptions and return INT64_MIN.
using A2foStage10FeatureChainStep = std::int64_t (*)(void*, std::uint32_t, std::size_t);
bool a2fo_rs_stage10_feature_chain_contains(void*, A2foStage10FeatureChainStep);
}
