#pragma once
#include "model_dependencies.hpp"
#include "asset_snapshot.hpp"
#include <map>
#include <utility>
namespace a2fo::native11 {
class DependencyPlan final {
public:
    const Frame& frame() const noexcept {return frame_;}
    const std::vector<std::string>& textures() const noexcept {return textures_;}
    const auto& models() const noexcept {return models_;}
    static std::shared_ptr<const ModelAsset> resolve(void*,std::uint32_t,std::uint64_t);
private:
    friend bool build_dependency_plan(const Frame&,ResolveSnapshotModel,void*,ModelDependencyCache&,ParseSod,
        std::shared_ptr<const DependencyPlan>&,std::string&) noexcept;
    DependencyPlan()=default;
    Frame frame_{};
    std::map<std::pair<std::uint32_t,std::uint64_t>,std::shared_ptr<const ModelAsset>> models_;
    std::vector<std::string> textures_;
};
// Caller synchronizes the resolver/cache separately. Uses owned model versions,
// never engine addresses. Static pristine dependencies only, not live variants.
bool build_dependency_plan(const Frame&,ResolveSnapshotModel,void*,ModelDependencyCache&,ParseSod,
    std::shared_ptr<const DependencyPlan>& output,std::string& error) noexcept;
}
