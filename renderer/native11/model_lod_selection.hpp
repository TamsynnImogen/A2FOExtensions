#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace a2fo::native11 {
struct ModelLodSelectionInput {
    std::array<float,3> translation{},centre{};
    std::array<float,4> thresholds{};
    float radius=0;
    std::int32_t requested=-1,quality_cap=4;
    std::uint32_t level_count=0;
    bool force_highest=false;
};
// Native list ordinal, not a SOD name/face-count sort. UINT32_MAX denotes an
// empty control (native no-op). Caller supplies a fresh camera-space transform
// and context. This selection alone does not certify visibility or coverage.
// Exact x87 arithmetic order, caller PC/RC retained; no native reads or writes.
// Failure leaves output unchanged.
bool select_model_lod(const ModelLodSelectionInput&,std::uint32_t& index,
    std::string& error) noexcept;
} // namespace a2fo::native11
