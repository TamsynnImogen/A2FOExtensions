#ifndef A2FO_STAGE10_RGB_COMPLETION_FFI_H
#define A2FO_STAGE10_RGB_COMPLETION_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {

using A2foRgbCompletionStep = std::uint32_t (*)(
    void* context, std::uint32_t operation, std::size_t index);

// Modes match the discovery callback operations: root DDS publication (3),
// legacy folder publication (5), TGA candidate admission (6), and final
// candidate append (7). The callback owns native map iterators and storage.
bool a2fo_rs_stage10_rgb_traverse(
    std::uint32_t mode, A2foRgbCompletionStep step, void* context);

}

#endif
