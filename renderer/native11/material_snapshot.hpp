#pragma once
#include "asset_snapshot.hpp"
#include "material_observation.hpp"
#include "../../sdk/include/a2fo_native11_live_material_api.h"
#include "../../sdk/include/a2fo_native11_geometry_policy_api.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Immutable host-facing view of one sealed material observation batch.
// The frame's model names are per-instance opaque view IDs.
class BoundMaterialCapture final {
public:
    // Same immutable model/texture palette and instance mapping; pose/camera ignored.
    bool same_resources(const BoundMaterialCapture&) const noexcept;
    bool same_instance_contents(std::uint32_t instance,const BoundMaterialCapture&) const noexcept;
    bool resources_current(ResolveSnapshotModel,void*,ResolveTextureAsset,void*) const noexcept;
    std::uint32_t native_x87_control(std::uint32_t instance) const noexcept;
    std::uint32_t common_native_x87_control() const noexcept;
    bool export_instance(std::uint32_t,Object&,std::shared_ptr<const MaterialObservation>&,std::string&) const noexcept;
    const Frame& frame() const noexcept { return frame_; }
    std::uint64_t source_epoch() const noexcept { return base_->source_epoch(); }
    const std::shared_ptr<const BoundCapture>& base() const noexcept { return base_; }
    static std::int32_t A2FO_N11_CALL read_asset(void* context, std::uint32_t kind,
        const std::uint8_t* identifier, std::uint32_t length, std::uint32_t max_bytes,
        A2FO_N11_BytesSink sink, void* sink_context) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_texture(void* context,
        const std::uint8_t* model_identifier, std::uint32_t model_identifier_length,
        std::uint32_t part_index, const std::uint8_t* authored_texture,
        std::uint32_t authored_length, std::uint32_t max_identifier_bytes,
        A2FO_N11_BytesSink sink, void* sink_context) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_part_state(void*,const std::uint8_t*,std::uint32_t,
        std::uint32_t,const float[16],A2FO_N11_PartStateSink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_geometry_policy(void*,const std::uint8_t*,
        std::uint32_t,A2FO_N11_GeometryPolicySink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_live_material(void*,const std::uint8_t*,
        std::uint32_t,const A2FO_N11_MaterialSeed*,A2FO_N11_LiveMaterialSink,void*) noexcept;
private:
    friend bool build_bound_material_capture(const Frame&, std::uint64_t, ResolveSnapshotModel,
        void*, const std::map<std::uint32_t, std::shared_ptr<const MaterialObservation>>&,
        std::shared_ptr<const BoundMaterialCapture>&, std::string&) noexcept;
    BoundMaterialCapture() = default;
    struct Part { std::string authored, texture_id;std::uint32_t node_index=UINT32_MAX,group_index=UINT32_MAX;
        bool state_valid=false;PartState state{}; };
    struct View { std::string model_id; std::vector<Part> parts;std::uint32_t native_x87_control=0; };
    Frame frame_{};
    std::shared_ptr<const BoundCapture> base_;
    std::map<std::string, View> views_;
    std::map<std::string, std::shared_ptr<const TextureAsset>> textures_;
};

// Transactional: failure leaves output unchanged.
bool build_bound_material_capture(const Frame& input, std::uint64_t source_epoch,
    ResolveSnapshotModel resolve, void* resolve_context,
    const std::map<std::uint32_t, std::shared_ptr<const MaterialObservation>>& observations,
    std::shared_ptr<const BoundMaterialCapture>& output, std::string& error) noexcept;
} // namespace a2fo::native11
