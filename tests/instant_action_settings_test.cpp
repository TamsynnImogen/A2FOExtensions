#include "load_button_bounds.hpp"
#include "setup_details_line.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "instant_action_settings_test: " << message << '\n';
    std::exit(1);
}

}  // namespace

int main() {
    using a2fo::instant_action_settings::Bounds;
    using a2fo::instant_action_settings::effective_load_bounds;

    const Bounds save{376, 415, 505, 440};

    const auto valid = effective_load_bounds({376, 446, 505, 471}, save);
    require(valid.valid && !valid.repaired,
            "valid Load bounds should remain authoritative");
    require(valid.bounds.left == 376 && valid.bounds.bottom == 471,
            "valid Load bounds should be preserved");

    const auto missing_extent =
        effective_load_bounds({376, 446, 0, 0}, save);
    require(missing_extent.valid && missing_extent.repaired,
            "missing Load extents should be repaired");
    require(missing_extent.bounds.right == 505 &&
                missing_extent.bounds.bottom == 471,
            "Load should inherit the Save control dimensions");

    const auto missing_all = effective_load_bounds({}, save);
    require(missing_all.valid && missing_all.repaired,
            "a wholly empty Load rectangle should use the adjacent Save row");
    require(missing_all.bounds.left == 376 && missing_all.bounds.top == 446 &&
                missing_all.bounds.right == 505 &&
                missing_all.bounds.bottom == 471,
            "the Fleet Ops form's six-pixel row gap should be retained");

    const auto misleading =
        effective_load_bounds({0, 0, 129, 25}, save);
    require(misleading.valid && misleading.repaired &&
                misleading.bounds.left == 376 &&
                misleading.bounds.top == 446,
            "a formally valid rectangle outside the Load row should repair");

    const auto unavailable = effective_load_bounds({}, {});
    require(!unavailable.valid,
            "invalid Load and Save rectangles should disable the fallback");

    require(a2fo::instant_action_settings::contains(
                missing_all.bounds, 376, 446),
            "top-left boundary should be included");
    require(a2fo::instant_action_settings::contains(
                missing_all.bounds, 504, 470),
            "last interior pixel should be included");
    require(!a2fo::instant_action_settings::contains(
                missing_all.bounds, 505, 470) &&
                !a2fo::instant_action_settings::contains(
                    missing_all.bounds, 504, 471),
            "right and bottom boundaries should be excluded");

    require(a2fo::instant_action_settings::is_load_settings_caption(
                "Load Settings") &&
                a2fo::instant_action_settings::is_load_settings_caption(
                    "&LOAD_SETTINGS"),
            "the Fleet Ops child-button caption should match robustly");
    require(!a2fo::instant_action_settings::is_load_settings_caption(
                "Save Settings") &&
                !a2fo::instant_action_settings::is_load_settings_caption(
                    "Load Defaults"),
            "unrelated setup commands must not match Load Settings");

    std::array<std::uint8_t, 4> decoded{};
    const char spaced[] = "setupDetails = 001aB2ff\r\nnext = 1\r\n";
    const auto spaced_result =
        a2fo::instant_action_settings::decode_setup_details_line(
            spaced, sizeof(spaced) - 1, decoded.data(), decoded.size());
    require(spaced_result.decoded && decoded[0] == 0x00 &&
                decoded[1] == 0x1a && decoded[2] == 0xb2 &&
                decoded[3] == 0xff,
            "the spaced setupDetails payload should decode byte-aligned");
    require(spaced_result.next_line_offset == 25,
            "the reader cursor should advance across CRLF");

    const char compact[] = "\tsetupDetails=0123\n";
    std::array<std::uint8_t, 2> compact_decoded{};
    require(a2fo::instant_action_settings::decode_setup_details_line(
                compact, sizeof(compact) - 1, compact_decoded.data(),
                compact_decoded.size()).decoded &&
                compact_decoded[0] == 0x01 && compact_decoded[1] == 0x23,
            "the decoder should tolerate compact and tab-indented lines");

    const char wrong_label[] = "otherDetails = 001aB2ff\r\n";
    const char truncated[] = "setupDetails = 001aB2\r\n";
    const char invalid_hex[] = "setupDetails = 001xB2ff\r\n";
    require(!a2fo::instant_action_settings::decode_setup_details_line(
                 wrong_label, sizeof(wrong_label) - 1, decoded.data(),
                 decoded.size()).decoded &&
                !a2fo::instant_action_settings::decode_setup_details_line(
                 truncated, sizeof(truncated) - 1, decoded.data(),
                 decoded.size()).decoded &&
                !a2fo::instant_action_settings::decode_setup_details_line(
                 invalid_hex, sizeof(invalid_hex) - 1, decoded.data(),
                 decoded.size()).decoded,
            "unrelated, truncated, and invalid payloads must be rejected");

    std::cout << "instant action settings tests passed\n";
    return 0;
}
