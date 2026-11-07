#pragma once
#include "material_snapshot.hpp"
#include "world_lighting.hpp"
#include "world_service.hpp"
#include "../../sdk/include/a2fo_native11_geometry_policy_api.h"
#include "../../sdk/include/a2fo_native11_model_scope_api.h"
#include "../../sdk/include/a2fo_native11_live_material_api.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Publish without mutable aliases to the packet, capture, or asset owners.
struct OwnedWorldFrame {
    std::shared_ptr<const BoundMaterialCapture> materials;
    LightingFrame lighting;
    std::uint32_t light_camera=0;
    std::uint64_t light_generation=0;
    bool complete_casters=false;
};

// Externally serialized and owner-thread-only. The caller keeps the API5 DLL
// loaded and must call close() on the creating thread before destruction.
// No renderer API is called from the destructor or a loader-lock context.
// World packets must carry observed part states and complete caster coverage.
class WorldRenderSession final {
public:
    WorldRenderSession()=default;
    WorldRenderSession(const WorldRenderSession&)=delete;
    WorldRenderSession& operator=(const WorldRenderSession&)=delete;
    WorldRenderSession(WorldRenderSession&&)=delete;
    WorldRenderSession& operator=(WorldRenderSession&&)=delete;
    ~WorldRenderSession()=default;

    bool initialize(const A2FO_N11_API5&,std::uint32_t width,std::uint32_t height,
        std::string& error) noexcept;
    bool initialize_shared(const A2FO_N11_API6&,std::uint32_t width,std::uint32_t height,
        std::string& error) noexcept;
    bool initialize_all_parts(const A2FO_N11_API9&,std::uint32_t width,
        std::uint32_t height,std::string& error) noexcept;
    bool initialize_native_geometry(const A2FO_N11_API10&,std::uint32_t width,
        std::uint32_t height,std::string& error) noexcept;
    bool initialize_live_materials(const A2FO_N11_API11&,std::uint32_t width,
        std::uint32_t height,std::string& error,bool share_geometry=false) noexcept;
    bool render(std::shared_ptr<const OwnedWorldFrame>,std::string& error) noexcept;
    bool submit_live_materials(std::shared_ptr<const BoundMaterialCapture>,bool preview,
        const float ambient[3],bool (*checkpoint)(void*) noexcept,void*,std::string&) noexcept;
    bool resize(std::uint32_t width,std::uint32_t height,std::string& error) noexcept;
    bool close(std::string& error) noexcept;
    bool readback(std::vector<std::uint8_t>& bgra,std::uint32_t& width,
        std::uint32_t& height,std::string& error) noexcept;
    bool usage(A2FO_N11_Usage&,std::string& error) noexcept;
    std::shared_ptr<const OwnedWorldFrame> current() const noexcept;
    std::uint64_t accepted_generation() const noexcept;
    std::uint64_t accepted_material_generation() const noexcept;

private:
    static std::int32_t A2FO_N11_CALL read_asset(void*,std::uint32_t,
        const std::uint8_t*,std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_texture(void*,const std::uint8_t*,
        std::uint32_t,std::uint32_t,const std::uint8_t*,std::uint32_t,
        std::uint32_t,A2FO_N11_BytesSink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_part_state(void*,const std::uint8_t*,
        std::uint32_t,std::uint32_t,const float[16],A2FO_N11_PartStateSink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_geometry_policy(void*,const std::uint8_t*,
        std::uint32_t,A2FO_N11_GeometryPolicySink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_live_material(void*,const std::uint8_t*,
        std::uint32_t,const A2FO_N11_MaterialSeed*,A2FO_N11_LiveMaterialSink,void*) noexcept;
    static void A2FO_N11_CALL log(void*,const char*,std::uint32_t) noexcept;
    bool available() const noexcept;
    std::shared_ptr<const BoundMaterialCapture> callback_materials() const noexcept;
    A2FO_N11_API5 api_{};
    A2FO_N11_API11 live_api_{};
    bool live_enabled_=false;
    A2FO_N11_Session session_=0;
    std::uint32_t owner_thread_=0;
    bool busy_=false;
    std::uint64_t accepted_generation_=0;
    std::uint64_t accepted_material_generation_=0;
    std::uint64_t attempted_material_generation_=0;
    std::shared_ptr<const BoundMaterialCapture> current_material_;
    std::shared_ptr<const BoundMaterialCapture> pending_material_;
    std::shared_ptr<const OwnedWorldFrame> current_;
    std::shared_ptr<const OwnedWorldFrame> pending_;
    std::string last_log_;
};
} // namespace a2fo::native11
