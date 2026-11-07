#include "setup_details_line.hpp"
#include "../../rust/ui_ffi.h"
namespace a2fo::instant_action_settings {
SetupDetailsDecodeResult decode_setup_details_line(const char *text, std::size_t text_size,
                                                   std::uint8_t *output,
                                                   std::size_t output_size) noexcept {
    if (!text || !output || !output_size)
        return {};
    const std::size_t r = a2fo_rs_ui_decode_details(reinterpret_cast<const std::uint8_t *>(text),
                                                    text_size, output, output_size);
    return {r != 0, r ? r - 1 : 0};
}
} // namespace a2fo::instant_action_settings
