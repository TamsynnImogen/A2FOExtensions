#include "../renderer/native11/model_node_template_cache.hpp"
#include "../renderer/native11/model_lod_template.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Fixture {
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> data=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<const ModelNodeTemplate> snapshot;
    Fixture(std::uint64_t serial=11){
        model->info.database=7;model->info.serial=serial;
        std::strcpy(model->info.name,"root.sod");model->bytes={42};model->info.byte_count=1;
        AuthoredPoseNode authored_node;authored_node.name="root";
        authored->nodes.push_back(authored_node);
        data->root_address=0x1000;
        ModelNodeTemplateNode node;node.address=0x1000;node.name="root";
        data->nodes.push_back(node);
        refresh();
    }
    void refresh(){snapshot=std::make_shared<const ModelNodeTemplate>(
        ModelNodeTemplate{model,authored,data});}
};
Fixture with_lods(std::uint64_t serial){
    Fixture f(serial);
    AuthoredPoseNode first;first.name="first";first.parent=0;
    AuthoredPoseNode second;second.name="second";second.parent=0;
    f.authored->nodes.push_back(first);f.authored->nodes.push_back(second);
    ModelNodeTemplateNode first_node;first_node.address=0x1100;first_node.name="first";
    first_node.parent=0;
    ModelNodeTemplateNode second_node;second_node.address=0x1200;second_node.name="second";
    second_node.parent=0;
    f.data->nodes.push_back(first_node);f.data->nodes.push_back(second_node);
    auto lods=std::make_shared<ModelLodTemplateData>();
    ModelLodControl control;control.node_index=0;control.centre={1,2,3};control.radius=4;
    control.levels={{2,200},{1,50}};lods->controls.push_back(control);
    f.data->lods=lods;f.refresh();return f;
}
void reject_publish(ModelNodeTemplateCache& cache,const Fixture& f,const char* why){
    const auto before_size=cache.size(),before_bytes=cache.bytes();
    std::string error;
    need(!cache.publish(f.snapshot,error)&&!error.empty(),why);
    need(cache.size()==before_size&&cache.bytes()==before_bytes,
         "failed publication retains cache accounting");
}
}
int main(){
    std::string error;ModelNodeTemplateCache cache;
    Fixture first;
    need(cache.publish(first.snapshot,error)&&error.empty()&&cache.size()==1&&cache.bytes()>0,
         "first immutable load template publication");
    const auto cost=cache.bytes();
    std::shared_ptr<const ModelNodeTemplate> output;
    need(cache.find(first.model,output,error)&&output->model==first.model&&
         output->authored==first.authored&&output->data==first.data,
         "find pins model, authored and template owners");
    auto old=output;
    first.refresh();
    need(cache.publish(first.snapshot,error)&&cache.size()==1&&cache.bytes()==cost,
         "same owner tuple is idempotent across snapshot wrappers");
    auto altered=first;
    altered.authored=std::make_shared<AuthoredPoseData>(*first.authored);altered.refresh();
    reject_publish(cache,altered,"same model cannot replace authored owner");
    altered=first;
    altered.data=std::make_shared<ModelNodeTemplateData>(*first.data);altered.refresh();
    reject_publish(cache,altered,"same model cannot replace template owner");
    Fixture collision(11);
    reject_publish(cache,collision,"live same-serial different-model collision");
    need(!cache.find(collision.model,output,error)&&output==old&&
         error.find("collides")!=std::string::npos,
         "colliding lookup leaves output untouched");
    auto missing=std::make_shared<ModelAsset>(*first.model);missing->info.serial=90;
    need(!cache.find(missing,output,error)&&output==old,
         "cache miss leaves output untouched");
    cache.clear();need(cache.size()==0&&cache.bytes()==0&&old->model==first.model&&
        old->authored==first.authored&&old->data==first.data,
        "clear preserves previously returned snapshot owners");

    ModelNodeTemplateCache one(1);Fixture second(12);
    need(one.publish(first.snapshot,error),"single-entry first publication");
    reject_publish(one,second,"live entry capacity refuses second model");
    ModelNodeTemplateCache too_small(64,cost-1);
    reject_publish(too_small,first,"metadata byte budget refuses oversized entry");
    ModelNodeTemplateCache exact(64,cost);
    need(exact.publish(first.snapshot,error)&&exact.bytes()==cost,
         "exact metadata byte budget accepted");
    Fixture richer(13);AuthoredPoseChannel channel;channel.node_index=0;channel.channel_type=5;
    channel.period=1;channel.key_width=1;channel.keys.assign(16,1);
    richer.authored->channels.push_back(channel);richer.data->nodes[0].name+="-extended";
    richer.refresh();
    ModelNodeTemplateCache measured;
    need(measured.publish(richer.snapshot,error)&&measured.bytes()>cost+
         16*sizeof(float),"channel key floats and names contribute to accounting");

    Fixture lod=with_lods(14);
    ModelNodeTemplateCache lod_cache;
    need(lod_cache.publish(lod.snapshot,error)&&lod_cache.size()==1&&
         lod_cache.bytes()>cost&&lod.data->lods->controls[0].levels[0].face_count==200,
         "LOD control and level metadata counted without reordering");
    const auto lod_cost=lod_cache.bytes();
    ModelNodeTemplateCache lod_small(64,lod_cost-1);
    reject_publish(lod_small,lod,"LOD metadata budget enforced");
    ModelNodeTemplateCache lod_exact(64,lod_cost);
    need(lod_exact.publish(lod.snapshot,error)&&lod_exact.bytes()==lod_cost,
         "exact LOD metadata budget accepted");
    std::shared_ptr<const ModelNodeTemplate> lod_found;
    need(lod_cache.find(lod.model,lod_found,error)&&
         lod_found->data->lods==lod.data->lods,
         "lookup pins copied LOD metadata with snapshot");
    lod_cache.clear();
    need(lod_found->data->lods->controls[0].levels[0].node_index==2,
         "clear retains returned LOD snapshot values");

    ModelNodeTemplateCache lod_aggregate(2,lod_cost+cost-1);
    need(lod_aggregate.publish(lod.snapshot,error),"aggregate LOD first entry");
    reject_publish(lod_aggregate,first,"aggregate copied metadata budget keeps live entry");
    auto reject_lod=[&](std::uint64_t serial,auto edit,const char* why){
        Fixture invalid=with_lods(serial);
        auto values=std::make_shared<ModelLodTemplateData>(*invalid.data->lods);
        edit(*values);invalid.data->lods=values;invalid.refresh();
        reject_publish(cache,invalid,why);
    };
    reject_lod(30,[](auto& v){v.controls[0].node_index=3;},"out-of-range LOD control");
    reject_lod(31,[](auto& v){v.controls.push_back(v.controls[0]);},"duplicate LOD control");
    reject_lod(32,[](auto& v){v.controls[0].levels[0].node_index=3;},"out-of-range LOD level");
    reject_lod(33,[](auto& v){v.controls[0].levels[1].node_index=2;},"duplicate LOD level");
    reject_lod(34,[](auto& v){v.controls[0].levels[0].node_index=0;},"non-child LOD level");
    reject_lod(35,[](auto& v){v.controls[0].centre[1]=NAN;},"nonfinite LOD centre");
    reject_lod(36,[](auto& v){v.controls[0].radius=-1;},"negative LOD radius");
    reject_lod(37,[](auto& v){v.controls[0].radius=NAN;},"nonfinite LOD radius");
    reject_lod(38,[](auto& v){v.controls[0].levels.assign(4097,{1,1});},"aggregate LOD level limit");

    Fixture bad(20);bad.model->info.byte_count=2;bad.refresh();
    reject_publish(cache,bad,"model byte count mismatch refused");
    bad=Fixture(21);bad.data->root_address=0x2000;bad.refresh();
    reject_publish(cache,bad,"unmapped root refused");
    bad=Fixture(22);bad.data->damage=1;bad.refresh();
    reject_publish(cache,bad,"out-of-range role refused");
    bad=Fixture(23);bad.model->info.database=0;bad.refresh();
    reject_publish(cache,bad,"zero model database refused");
    bad=Fixture(24);bad.authored->nodes.clear();bad.refresh();
    reject_publish(cache,bad,"authored/template node count mismatch refused");

    std::weak_ptr<const ModelAsset> weak;
    ModelNodeTemplateCache expiring;
    {
        Fixture temporary(50);weak=temporary.model;
        need(expiring.publish(temporary.snapshot,error)&&expiring.size()==1,
             "temporary model published");
    }
    need(weak.expired()&&expiring.size()==0&&expiring.bytes()==0,
         "expired model owner pruned before accounting queries");
    std::weak_ptr<const ModelAsset> lod_weak;
    ModelNodeTemplateCache lod_expiring;
    {
        Fixture temporary=with_lods(51);lod_weak=temporary.model;
        need(lod_expiring.publish(temporary.snapshot,error)&&lod_expiring.size()==1,
             "temporary LOD model published");
    }
    need(lod_weak.expired()&&lod_expiring.size()==0&&lod_expiring.bytes()==0,
         "LOD metadata does not retain expired model owner");
    std::puts("PASS native11 model node template cache: weak owners, identity and bounded accounting");
}
