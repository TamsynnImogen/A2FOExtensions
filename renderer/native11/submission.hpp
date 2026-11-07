#pragma once
#include "renderer.hpp"
#include "content.hpp"
#include "scene_capture.hpp"
#include <memory>
#include <string>

namespace a2fo::native11 {
struct StaticMaterialPolicy {
    bool relight_fullbright = false;
    float retained_emission = 0.f;
    bool share_geometry = false;
    bool all_model_parts = false;
    bool native_geometry = false;
};
// Internal single-render-thread frontend. Source identifiers are resolved only
// by the supplied reader. The caller advances source_epoch whenever its asset
// roots/content change. Device recreation is detected independently.
// This stage submits pristine static content from owned captures to a private
// native target. It never presents or claims animation/scale/effect parity.
class SceneSubmission {
public:
    SceneSubmission(Renderer&, AssetReader, StaticMaterialPolicy policy = {});
    ~SceneSubmission();
    SceneSubmission(const SceneSubmission&)=delete;
    SceneSubmission& operator=(const SceneSubmission&)=delete;
    HRESULT render_preview(const Frame&, std::uint64_t source_epoch,
                           const Light*,std::uint32_t light_count,
                           const float ambient[3],std::string& error,
                           const ShadowSettings& shadows={}) noexcept;
    // Resolve and upload live materials without drawing, clearing or presenting.
    HRESULT prepare_live_materials(const Frame&,std::uint64_t source_epoch,
                                   std::string& error) noexcept;
    // Clear before destroying the referenced Renderer. Names/resources are not
    // reused across source epochs or successful renderer reinitialization.
    void clear() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};
} // namespace a2fo::native11
