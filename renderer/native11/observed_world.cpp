#include "observed_world.hpp"
#include <map>
#include <utility>

namespace a2fo::native11 {
namespace {
void fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}
}
struct Owners {
    std::map<std::pair<std::uint32_t,std::uint64_t>,std::shared_ptr<const ModelAsset>> models;
};
std::shared_ptr<const ModelAsset> resolve(void* context,std::uint32_t database,
    std::uint64_t serial) {
    const auto& models=static_cast<Owners*>(context)->models;
    const auto found=models.find({database,serial});
    return found==models.end()?nullptr:found->second;
}
const BoundMaterialCapture* find(const BoundMaterialCapture& visible,
    const std::vector<std::shared_ptr<const BoundMaterialCapture>>& candidates,
    const CoveredWorldMember& covered) noexcept {
    const auto has=[&](const BoundMaterialCapture& capture) {
        const auto& frame=capture.frame();
        for(std::size_t i=0;i<frame.object_count;++i)
            if(frame.objects[i].entity==covered.member.entity&&
               frame.objects[i].instance==covered.member.instance)return true;
        return false;
    };
    if(covered.visible&&has(visible))return &visible;
    for(const auto& candidate:candidates)if(candidate&&has(*candidate))return candidate.get();
    return nullptr;
}
}

bool ObservedWorldAssembler::update(std::shared_ptr<const BoundMaterialCapture> visible,
    const WorldLightCapture& world,
    const std::vector<std::shared_ptr<const BoundMaterialCapture>>& candidates,
    DomainEvidence domains,std::uint64_t fresh_epoch,std::string& error) noexcept {
    if(!visible){fail(error,"missing visible material capture");return false;}
    CoverageReport audited;
    if(!audit_coverage(*visible,world,candidates,audited,error))return false;
    report_=std::move(audited);
    if(!report_.game_objects_complete){fail(error,"checked world GameObject geometry is incomplete");return false;}
    if(!domains.no_game_features||!domains.no_background_casters||!domains.no_effect_casters){
        fail(error,"non-GameObject caster domains must be certified absent");return false;
    }
    try {
        auto frame=std::make_unique<Frame>(visible->frame());
        frame->object_count=0;
        frame->requires_part_states=true;
        frame->complete_casters=true;
        Owners owners;
        std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
        for(const auto& covered:report_.covered){
            const auto* capture=find(*visible,candidates,covered);
            if(!capture){fail(error,"covered world member has no owned capture");return false;}
            Object raw{};std::shared_ptr<const MaterialObservation> observation;
            if(!capture->export_instance(covered.member.instance,raw,observation,error))return false;
            raw.shadow_only=!covered.visible;
            const auto key=std::make_pair(raw.database,raw.model_serial);
            const auto inserted=owners.models.emplace(key,observation->model);
            if(!inserted.second&&inserted.first->second!=observation->model){
                fail(error,"conflicting owned model for database and serial");return false;
            }
            if(!observations.emplace(raw.instance,std::move(observation)).second){
                fail(error,"duplicate owned world instance");return false;
            }
            frame->objects[frame->object_count++]=raw;
        }
        return stream_.update(*frame,fresh_epoch,resolve,&owners,observations,error);
    }catch(...){fail(error,"observed world assembly allocation failure");return false;}
}

bool ObservedWorldAssembler::clear() noexcept {
    if(!stream_.clear())return false;
    report_=CoverageReport{};
    return true;
}
} // namespace a2fo::native11
