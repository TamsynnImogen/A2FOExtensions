#include "../renderer/native11/model_lod_visibility.hpp"
#include "../renderer/native11/model_lod_template.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
constexpr NativeMatrix34 identity{{1,0,0,0,1,0,0,0,1,0,0,0}};
std::uint16_t control(){std::uint16_t cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
struct Fixture {
    std::shared_ptr<ModelAsset> asset=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> data=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<ModelLodTemplateData> lods=std::make_shared<ModelLodTemplateData>();
    ModelNodeTemplate baseline;
    OwnedAnimationPose pose;
    ModelLodContext context;
    std::vector<std::uint32_t> flags;
    std::vector<std::uint8_t> class_write_masks;
    Fixture(){
        asset->info.database=7;asset->info.serial=11;
        std::strcpy(asset->info.name,"lod.sod");asset->bytes={1};asset->info.byte_count=1;
        baseline.model=asset;baseline.authored=authored;baseline.data=data;data->lods=lods;
        const char* names[]={"root","empty","high","nested","control","low","other","low_child","nested_high","nested_low"};
        data->nodes.resize(10);authored->nodes.resize(10);pose.local.assign(10,identity);
        pose.scales.assign(10,1.f);flags.assign(10,0);class_write_masks.assign(10,0);
        for(unsigned i=0;i<10;++i){
            auto& node=data->nodes[i];node.address=0x1000+0x100*i;node.name=names[i];
            authored->nodes[i].name=names[i];
            node.parent=authored->nodes[i].parent=UINT32_MAX;
        }
        auto child=[&](unsigned parent,unsigned id,unsigned sibling){
            auto& node=data->nodes[id];node.parent=authored->nodes[id].parent=parent;
            node.parent_address=data->nodes[parent].address;
            node.next_sibling=sibling==UINT32_MAX?UINT32_MAX:sibling;
            node.sibling_address=sibling==UINT32_MAX?0:data->nodes[sibling].address;
        };
        data->root_address=data->nodes[0].address;
        data->nodes[0].first_child=4;data->nodes[0].child_address=data->nodes[4].address;
        child(0,4,6);child(0,6,1);child(0,1,UINT32_MAX);
        data->nodes[4].first_child=5;data->nodes[4].child_address=data->nodes[5].address;
        child(4,5,2);child(4,2,UINT32_MAX);
        data->nodes[5].first_child=7;data->nodes[5].child_address=data->nodes[7].address;
        child(5,7,UINT32_MAX);
        data->nodes[2].first_child=3;data->nodes[2].child_address=data->nodes[3].address;
        child(2,3,UINT32_MAX);
        data->nodes[3].first_child=9;data->nodes[3].child_address=data->nodes[9].address;
        child(3,9,8);child(3,8,UINT32_MAX);
        for(unsigned id:{2u,5u,8u,9u})data->nodes[id].flags=flags[id]=3;
        flags[6]=1; // own hidden flag does not prune its sibling.
        lods->controls.push_back({4,{0,0,0},0,{{5,100},{2,200}}});
        lods->controls.push_back({3,{0,0,0},0,{{9,5},{8,9}}});
        lods->controls.push_back({1,{0,0,0},0,{}});
        pose.instance=101;pose.database=7;pose.model_serial=11;
        pose.local[0][9]=1;pose.scales[0]=2; // scale basis, not translation.
        pose.local[4][9]=2;
        context.engine=0x200;context.camera=0x300;context.render_context=0x400;
        context.instance=101;context.database=7;
        context.native_x87_control=encode_native_x87(0x037f);
        context.placement=identity;context.placement[9]=3;
        context.view=identity;context.thresholds={{100,50,20,10}};
    }
    bool run(ModelLodVisibility& output,std::string& error){
        return evaluate_model_lod_visibility(baseline,pose,context,flags,class_write_masks,output,error);
    }
};
void reject(const char* why,const std::function<void(Fixture&)>& edit){
    Fixture f;edit(f);ModelLodVisibility output;
    output.flags={77};output.selected_nodes={88};output.visible={true};
    output.world={identity};const auto expected=output;std::string error="stale";
    need(!f.run(output,error),why);
    need(output.flags==expected.flags&&output.selected_nodes==expected.selected_nodes&&
         output.visible==expected.visible&&output.world==expected.world&&
         !error.empty()&&error!="stale","failure leaves output atomic and explains cause");
}
void cases(){
    Fixture f;ModelLodVisibility selected;std::string error="stale";
    const auto caller=control();
    need(f.run(selected,error)&&error.empty(),"primary nested LOD evaluation");
    need(selected.world.size()==10&&selected.world[0][9]==4&&
         selected.world[0][0]==2&&selected.world[4][9]==8,
         "root local translation, native scale and world placement compose in row order");
    need(selected.selected_nodes==std::vector<std::uint32_t>({2,8,UINT32_MAX})&&
         selected.flags[2]==0&&selected.flags[5]==3&&
         selected.flags[8]==0&&selected.flags[9]==3,
         "native child order chooses tail regardless of file index and nested control selects");
    need(selected.visible[4]&&selected.visible[2]&&selected.visible[3]&&selected.visible[8]&&
         !selected.visible[5]&&!selected.visible[7]&&!selected.visible[9]&&
         !selected.visible[6]&&selected.visible[1],
         "own hide, ancestor prune and empty control visibility");
    Fixture second;second.baseline=f.baseline;
    second.pose.instance=102;second.context.instance=102;
    second.context.placement[9]=100;
    ModelLodVisibility distant;need(second.run(distant,error)&&
         distant.world[4][9]==105&&
         distant.selected_nodes==std::vector<std::uint32_t>({5,UINT32_MAX,UINT32_MAX})&&
         distant.visible[5]&&distant.visible[7]&&!distant.visible[2]&&
         selected.selected_nodes[0]==2,
         "two independently placed objects select different native ordinals");
    f.flags[0]=2;ModelLodVisibility pruned;
    need(f.run(pruned,error)&&
         pruned.selected_nodes==std::vector<std::uint32_t>({UINT32_MAX,UINT32_MAX,UINT32_MAX})&&
         !pruned.visible[4]&&!pruned.visible[5]&&!pruned.visible[2]&&
         pruned.flags[2]==3&&pruned.flags[5]==3,
         "ancestor prune skips every nested LOD selection");
    f.flags[0]=0;f.flags[4]=3;
    need(f.run(pruned,error)&&pruned.selected_nodes[0]==UINT32_MAX&&
         pruned.flags[2]==3&&pruned.flags[5]==3&&
         !pruned.visible[4]&&!pruned.visible[2],
         "hidden pruned control is safe without a previous selection");
    need(control()==caller,"evaluation restores caller x87 control word");
    reject("missing LOD metadata",[](Fixture& x){x.data->lods.reset();});
    reject("unknown original CW",[](Fixture& x){x.context.native_x87_control=0;});
    reject("invalid original CW",[](Fixture& x){x.context.native_x87_control=0x10100;});
    reject("instance mismatch",[](Fixture& x){x.context.instance++;});
    reject("database mismatch",[](Fixture& x){x.pose.database++;});
    reject("serial mismatch",[](Fixture& x){x.pose.model_serial++;});
    reject("class flag count",[](Fixture& x){x.flags.pop_back();});
    reject("pose local count",[](Fixture& x){x.pose.local.pop_back();});
    reject("authored node count",[](Fixture& x){x.authored->nodes.pop_back();});
    reject("LOD role alias",[](Fixture& x){x.flags[5]&=~1u;});
    reject("LOD class write alias despite unchanged flags",[](Fixture& x){x.class_write_masks[5]=2;});
    reject("LOD baseline alias",[](Fixture& x){x.data->nodes[2].flags&=~2u;});
    reject("hidden control unpruned",[](Fixture& x){x.flags[4]=1;});
    reject("cycle",[](Fixture& x){x.data->nodes[9].first_child=3;});
    reject("disconnected node",[](Fixture& x){x.data->nodes[6].next_sibling=UINT32_MAX;});
    reject("invalid child index",[](Fixture& x){x.data->nodes[4].first_child=99;});
    reject("nonfinite scale",[](Fixture& x){x.pose.scales[0]=std::numeric_limits<float>::quiet_NaN();});
    reject("missing root",[](Fixture& x){x.data->root_address=0;});
    reject("duplicate control",[](Fixture& x){x.lods->controls.push_back(x.lods->controls[0]);});
    Fixture deep;deep.lods->controls.clear();deep.data->nodes.clear();deep.authored->nodes.clear();
    deep.pose.local.clear();deep.pose.scales.clear();deep.flags.clear();deep.class_write_masks.clear();
    for(unsigned i=0;i<257;++i){
        ModelNodeTemplateNode node;node.address=0x1000+i*0x10;node.name="n"+std::to_string(i);
        node.parent=i?i-1:UINT32_MAX;node.first_child=i<256?i+1:UINT32_MAX;
        deep.data->nodes.push_back(node);
        AuthoredPoseNode authored;authored.name=node.name;authored.parent=node.parent;
        deep.authored->nodes.push_back(authored);
        deep.pose.local.push_back(identity);deep.pose.scales.push_back(1);deep.flags.push_back(0);
        deep.class_write_masks.push_back(0);
    }
    deep.data->root_address=deep.data->nodes[0].address;
    ModelLodVisibility prior;prior.flags={77};prior.visible={true};prior.selected_nodes={88};
    const auto unchanged=prior;error="stale";
    need(!deep.run(prior,error)&&prior.flags==unchanged.flags&&
         prior.visible==unchanged.visible&&prior.selected_nodes==unchanged.selected_nodes&&
         !error.empty()&&error!="stale","depth 257 rejected atomically");
    {
        NativeArithmeticScope outer(encode_native_x87(0x0b7f));
        const auto outer_cw=control();Fixture nested;ModelLodVisibility result;
        need(outer.valid()&&nested.run(result,error)&&control()==outer_cw,
             "nested evaluator restores enclosing x87 control word");
    }
    need(control()==caller,"all failures restore caller x87 control word");
}
}
int main(){cases();std::puts("PASS native11 model LOD visibility");}
