#include "material_stream.hpp"
#include <utility>

namespace a2fo::native11 {
namespace {
void fail(std::string& error, const char* message) noexcept {
    try { error = message; } catch (...) { error.clear(); }
}
}

bool MaterialFrameStream::update(const Frame& input, std::uint64_t fresh_epoch,
    ResolveSnapshotModel resolve, void* resolve_context,
    const std::map<std::uint32_t, std::shared_ptr<const MaterialObservation>>& observations,
    std::string& error) noexcept {
    if (active_) { fail(error, "material stream is active"); return false; }
    if (!fresh_epoch || fresh_epoch <= last_fresh_epoch_) {
        fail(error, "material stream fresh epoch must increase"); return false;
    }
    ++active_;
    std::shared_ptr<const BoundMaterialCapture> candidate;
    const auto source_epoch = current_ ? current_->source_epoch() : fresh_epoch;
    bool good = build_bound_material_capture(input, source_epoch, resolve, resolve_context,
        observations, candidate, error);
    if (good && current_ && !current_->same_resources(*candidate)) {
        good = build_bound_material_capture(input, fresh_epoch, resolve, resolve_context,
            observations, candidate, error);
    }
    if (good) {
        current_ = std::move(candidate);
        last_fresh_epoch_ = fresh_epoch;
        error.clear();
    }
    --active_;
    return good;
}

bool MaterialFrameStream::clear() noexcept {
    if (active_) return false;
    current_.reset();
    return true;
}

std::int32_t A2FO_N11_CALL MaterialFrameStream::read_asset(void* context, std::uint32_t kind,
    const std::uint8_t* id, std::uint32_t length, std::uint32_t maximum,
    A2FO_N11_BytesSink sink, void* sink_context) noexcept {
    if (!context) return 0;
    auto& self = *static_cast<MaterialFrameStream*>(context);
    ++self.active_;
    auto capture = self.current_;
    const auto result = capture ? BoundMaterialCapture::read_asset(
        const_cast<BoundMaterialCapture*>(capture.get()),kind,id,length,maximum,sink,sink_context) : 0;
    --self.active_;
    return result;
}

std::int32_t A2FO_N11_CALL MaterialFrameStream::resolve_texture(void* context,
    const std::uint8_t* model, std::uint32_t model_length, std::uint32_t part,
    const std::uint8_t* authored, std::uint32_t authored_length, std::uint32_t maximum,
    A2FO_N11_BytesSink sink, void* sink_context) noexcept {
    if (!context) return 0;
    auto& self = *static_cast<MaterialFrameStream*>(context);
    ++self.active_;
    auto capture = self.current_;
    const auto result = capture ? BoundMaterialCapture::resolve_texture(
        const_cast<BoundMaterialCapture*>(capture.get()),model,model_length,part,
        authored,authored_length,maximum,sink,sink_context) : 0;
    --self.active_;
    return result;
}

std::int32_t A2FO_N11_CALL MaterialFrameStream::resolve_part_state(void* context,
    const std::uint8_t* model, std::uint32_t length, std::uint32_t part,
    const float local_to_world[16], A2FO_N11_PartStateSink sink,
    void* sink_context) noexcept {
    if (!context) return 0;
    auto& self = *static_cast<MaterialFrameStream*>(context);
    ++self.active_;
    auto capture = self.current_;
    const auto result = capture ? BoundMaterialCapture::resolve_part_state(
        const_cast<BoundMaterialCapture*>(capture.get()),model,length,part,
        local_to_world,sink,sink_context) : 0;
    --self.active_;
    return result;
}
} // namespace a2fo::native11
