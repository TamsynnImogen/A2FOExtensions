#pragma once
#include "material_snapshot.hpp"

namespace a2fo::native11 {
// Externally synchronized, synchronous host-side stream. The caller must not
// concurrently enter any method or mutate published model/texture owners.
class MaterialFrameStream final {
public:
    bool update(const Frame& input, std::uint64_t fresh_epoch,
        ResolveSnapshotModel resolve, void* resolve_context,
        const std::map<std::uint32_t, std::shared_ptr<const MaterialObservation>>& observations,
        std::string& error) noexcept;
    std::shared_ptr<const BoundMaterialCapture> current() const noexcept { return current_; }
    // Releases this frame's owners; the last admitted fresh_epoch is retained.
    bool clear() noexcept;

    // Static SDK callbacks use this stream as context and retain the current
    // capture through the sink call. A callback may synchronously inspect current().
    static std::int32_t A2FO_N11_CALL read_asset(void*, std::uint32_t,
        const std::uint8_t*, std::uint32_t, std::uint32_t,
        A2FO_N11_BytesSink, void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_texture(void*,
        const std::uint8_t*, std::uint32_t, std::uint32_t,
        const std::uint8_t*, std::uint32_t, std::uint32_t,
        A2FO_N11_BytesSink, void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_part_state(void*,
        const std::uint8_t*, std::uint32_t, std::uint32_t,
        const float[16], A2FO_N11_PartStateSink, void*) noexcept;
private:
    std::shared_ptr<const BoundMaterialCapture> current_;
    std::uint64_t last_fresh_epoch_ = 0;
    unsigned active_ = 0;
};
} // namespace a2fo::native11
