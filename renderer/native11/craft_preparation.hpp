#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace a2fo::native11 {
// Inputs to the checked native Craft setup policy. No native pointer, cached
// instance flags or node state is interpreted by this CPU policy.
struct CraftPreparationInput {
    std::array<std::uint8_t,5> system_state{};
    std::uint8_t suppress_system_hiding=0;
    bool race_matches_borg_cache=false,use_hull_ratio=false;
    float hull_current=0,hull_maximum=0,hull_divisor=1;
};
struct PreparedCraftState {
    std::array<bool,5> systems_hidden{};
    bool borg_variant=false;
    float hull_fraction=1;
};
// Native setup writes flags in order 9c,9d,9e,a0,9f and a1/a4. This helper
// performs only the equivalent CPU arithmetic under an explicit native CW.
// It establishes no freshness, class, geometry, material or visibility scope.
// Invalid/unrepresentable inputs preserve output and return false.
bool prepare_craft_state(const CraftPreparationInput&,std::uint32_t native_x87_control,
    PreparedCraftState&,std::string& error) noexcept;
}
