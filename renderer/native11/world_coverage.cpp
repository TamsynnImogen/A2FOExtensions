#include "world_coverage.hpp"
#include <map>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_world=4096,max_observed=2048,max_candidates=16;
struct Seen {
    const BoundMaterialCapture* capture=nullptr;
    bool visible=false,shadow_only=false;
};
bool audit(const BoundMaterialCapture& visible,const WorldLightCapture& world,
    const std::vector<std::shared_ptr<const BoundMaterialCapture>>& candidates,
    CoverageReport& output,const char*& failure) {
    if (world.members.size()>max_world||world.members.size()!=world.sources.size()||
        world.members.size()!=world.class_owners.size()||candidates.size()>max_candidates) {
        failure="world coverage input bound or parallel ownership mismatch";return false;
    }
    const auto& vf=visible.frame();
    if (!vf.camera||!vf.generation||vf.object_count>max_observed||
        (vf.object_count&&!vf.requires_part_states)) {
        failure="invalid visible capture generation or part states";return false;
    }
    std::map<std::uint32_t,WorldMember> members;
    std::set<std::uint32_t> world_instances;
    for (std::size_t i=0;i<world.members.size();++i) {
        const auto& member=world.members[i];const auto& owner=world.class_owners[i];
        if (!member.entity||!member.object_class||!member.class_version||
            world.sources[i].source_id!=member.entity||!owner||
            owner->class_id!=member.object_class||owner->version!=member.class_version||
            !members.emplace(member.entity,member).second||
            (member.instance&&!world_instances.insert(member.instance).second)) {
            failure="invalid or duplicate checked world member";return false;
        }
    }
    std::map<std::uint32_t,Seen> observed;
    std::set<std::uint32_t> observed_instances;
    std::size_t combined=0;
    const auto add=[&](const BoundMaterialCapture& capture,bool is_visible)->bool {
        const auto& frame=capture.frame();
        if (frame.camera!=vf.camera||frame.generation!=vf.generation||
            frame.object_count>max_observed-combined||
            (frame.object_count&&!frame.requires_part_states)) {
            failure="stale or oversized geometry capture";return false;
        }
        combined+=frame.object_count;
        std::set<std::uint32_t> local_entities,local_instances;
        for (std::size_t i=0;i<frame.object_count;++i) {
            const auto& object=frame.objects[i];
            const auto member=members.find(object.entity);
            if (!object.entity||!object.instance||member==members.end()||
                member->second.instance!=object.instance||
                !local_entities.insert(object.entity).second||
                !local_instances.insert(object.instance).second||
                (is_visible&&object.shadow_only)) {
                failure="extra, duplicate, or mismatched geometry object";return false;
            }
            const auto prior=observed.find(object.entity);
            if (prior!=observed.end()) {
                if (!prior->second.visible||
                    !prior->second.capture->same_instance_contents(object.instance,capture)) {
                    failure="conflicting geometry observations";return false;
                }
            } else {
                if (!observed_instances.insert(object.instance).second) {
                    failure="ambiguous geometry instance";return false;
                }
                observed.emplace(object.entity,Seen{&capture,is_visible,object.shadow_only});
            }
        }
        return true;
    };
    if (!add(visible,true)) return false;
    for (const auto& candidate:candidates) {
        if (!candidate) { failure="null shadow candidate";return false; }
        if (!add(*candidate,false)) return false;
    }
    CoverageReport pending{};
    pending.world_objects=members.size();pending.observed_objects=observed.size();
    pending.missing.reserve(members.size()-observed.size());
    pending.covered.reserve(observed.size());
    for (const auto& [id,member]:members) {
        const auto found=observed.find(id);
        if (!member.instance||found==observed.end()) pending.missing.push_back(member);
        else pending.covered.push_back({member,found->second.visible,found->second.shadow_only});
    }
    pending.game_objects_complete=pending.missing.empty();
    output=std::move(pending);
    return true;
}
}

bool audit_coverage(const BoundMaterialCapture& visible,const WorldLightCapture& world,
    const std::vector<std::shared_ptr<const BoundMaterialCapture>>& shadow_candidates,
    CoverageReport& output,std::string& error) noexcept {
    try {
        const char* failure=nullptr;
        if (!audit(visible,world,shadow_candidates,output,failure)) {
            error=failure;return false;
        }
        error.clear();return true;
    } catch (...) {
        try { error="world coverage allocation or capture failure"; } catch (...) { error.clear(); }
        return false;
    }
}
} // namespace a2fo::native11
