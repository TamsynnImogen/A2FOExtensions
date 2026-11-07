#include "model_lod_visibility.hpp"
#include "model_lod_template.hpp"
#include "model_lod_selection.hpp"
#include "native_arithmetic.hpp"
#include <utility>

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
struct Pending {std::uint32_t node,parent,depth;bool pruned;};
}
bool evaluate_model_lod_visibility(const ModelNodeTemplate& baseline,
    const OwnedAnimationPose& pose,const ModelLodContext& context,
    const std::vector<std::uint32_t>& class_flags,
    const std::vector<std::uint8_t>& class_low_bit_writes,ModelLodVisibility& output,
    std::string& error) noexcept {
    try{
        NativeArithmeticScope arithmetic(context.native_x87_control);
        if(!arithmetic.valid()||!arithmetic.known()||!baseline.model||!baseline.authored||
           !baseline.data||!baseline.data->lods||!context.instance||!context.database||
           !context.camera||!context.render_context||!context.engine||
           context.instance!=pose.instance||context.database!=pose.database||
           pose.database!=baseline.model->info.database||!pose.model_serial||
           pose.model_serial!=baseline.model->info.serial)
            return fail(error,"invalid owned LOD source identity or original arithmetic unknown");
        const auto& nodes=baseline.data->nodes;const auto count=nodes.size();
        if(!count||count>4096||class_flags.size()!=count||class_low_bit_writes.size()!=count||pose.local.size()!=count||
           pose.scales.size()!=count||baseline.authored->nodes.size()!=count)
            return fail(error,"invalid owned LOD node shape");
        std::uint32_t root=UINT32_MAX;
        for(std::size_t i=0;i<count;++i){
            if(class_low_bit_writes[i]&~3u)return fail(error,"invalid class flag write mask");
            if(nodes[i].parent!=baseline.authored->nodes[i].parent||
               nodes[i].name!=baseline.authored->nodes[i].name)
                return fail(error,"owned LOD authored node identity mismatch");
            if(nodes[i].parent==UINT32_MAX){
                if(root!=UINT32_MAX||nodes[i].address!=baseline.data->root_address||
                   nodes[i].next_sibling!=UINT32_MAX)return fail(error,"invalid owned LOD root");
                root=static_cast<std::uint32_t>(i);
            }
        }
        if(root==UINT32_MAX)return fail(error,"owned LOD root missing");
        const auto& controls=baseline.data->lods->controls;
        if(controls.size()>count)return fail(error,"owned LOD control count exceeds nodes");
        std::vector<std::uint32_t> control_at(count,UINT32_MAX);
        std::vector<std::uint8_t> level_seen(count),seen(count);
        std::size_t total_levels=0;
        for(std::size_t i=0;i<controls.size();++i){
            const auto& control=controls[i];
            if(control.node_index>=count||control_at[control.node_index]!=UINT32_MAX||
               control.levels.size()>4096-total_levels)
                return fail(error,"invalid owned LOD control mapping");
            control_at[control.node_index]=static_cast<std::uint32_t>(i);
            total_levels+=control.levels.size();
            for(const auto& level:control.levels){
                const auto id=level.node_index;
                if(id>=count||level_seen[id]++||nodes[id].parent!=control.node_index||
                   (nodes[id].flags&3u)!=3u||(class_flags[id]&3u)!=3u||class_low_bit_writes[id])
                    return fail(error,"stateful LOD level/class flag alias or invalid mapping");
            }
        }
        ModelLodVisibility candidate;candidate.flags=class_flags;
        candidate.visible.resize(count);candidate.world.resize(count);
        candidate.selected_nodes.assign(controls.size(),UINT32_MAX);
        std::vector<Pending> pending{{root,UINT32_MAX,1,false}};
        std::size_t visited=0;
        while(!pending.empty()){
            const auto item=pending.back();pending.pop_back();
            if(item.node>=count||item.depth>256||seen[item.node]++||
               nodes[item.node].parent!=item.parent)
                return fail(error,"invalid owned LOD topology or depth");
            ++visited;const auto& node=nodes[item.node];NativeMatrix34 local;
            if(!native_matrix34_scale(pose.local[item.node],pose.scales[item.node],local,error)||
               !native_matrix34_multiply(local,item.parent==UINT32_MAX?context.placement:
                   candidate.world[item.parent],candidate.world[item.node],error))return false;
            const auto flags=candidate.flags[item.node];
            const auto control_index=control_at[item.node];
            if(!item.pruned&&control_index!=UINT32_MAX){
                const auto& control=controls[control_index];
                if((flags&1u)&&!(flags&2u)&&!control.levels.empty())
                    return fail(error,"hidden LOD control would retain a previous native selection");
                if(!(flags&1u)){
                    ModelLodSelectionInput selection;selection.centre=control.centre;
                    selection.radius=control.radius;selection.thresholds=context.thresholds;
                    selection.requested=context.requested;selection.quality_cap=context.quality_cap;
                    selection.force_highest=context.force_highest;
                    selection.level_count=static_cast<std::uint32_t>(control.levels.size());
                    if(!native_matrix34_camera_translation(candidate.world[item.node],context.view,
                           selection.translation,error))return false;
                    std::uint32_t selected=UINT32_MAX;
                    if(!select_model_lod(selection,selected,error))return false;
                    if(selected!=UINT32_MAX){const auto id=control.levels[selected].node_index;
                        candidate.selected_nodes[control_index]=id;candidate.flags[id]&=~3u;}
                }
            }
            candidate.visible[item.node]=!item.pruned&&!(flags&1u);
            if(node.next_sibling!=UINT32_MAX)
                pending.push_back({node.next_sibling,item.parent,item.depth,item.pruned});
            if(node.first_child!=UINT32_MAX)
                pending.push_back({node.first_child,item.node,item.depth+1,item.pruned||bool(flags&2u)});
        }
        if(visited!=count)return fail(error,"disconnected owned LOD topology");
        output=std::move(candidate);error.clear();return true;
    }catch(...){return fail(error,"owned LOD visibility evaluation failed");}
}
} // namespace a2fo::native11
