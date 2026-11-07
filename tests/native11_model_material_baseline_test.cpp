#include "../renderer/native11/model_material_baseline.hpp"
#include "../renderer/native11/model_material_baseline_cache.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t db=0x3000,root=0x4000,mesh1=0x4200,mesh2=0x4400;
constexpr std::uint32_t t1=0x6000,t2=0x6100,vector1=0x8000,vector2=0x8100;
constexpr MaterialBaselineReadProfile profile{0x9000,0x9100};
void need(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Fixture {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> owners;
    std::function<void(Fixture&)> on_read;
    std::uint32_t trigger=0;unsigned trigger_at=0,trigger_count=0,reads=0,resolves=0,replace_at=0;
    bool throwing=false;
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> data=std::make_shared<ModelNodeTemplateData>();
    ModelNodeTemplate source;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* p=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=p[k];
    }
    void str(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
    static bool read(void* raw,std::uint32_t address,void* out,std::size_t count){
        auto& f=*static_cast<Fixture*>(raw);++f.reads;
        if(f.throwing)throw std::runtime_error("reader");
        if(address==f.trigger&&++f.trigger_count==f.trigger_at&&f.on_read)f.on_read(f);
        auto* target=static_cast<std::uint8_t*>(out);
        for(std::size_t k=0;k<count;++k){
            if(k>UINT32_MAX-address)return false;
            const auto found=f.bytes.find(address+static_cast<std::uint32_t>(k));
            if(found==f.bytes.end())return false;
            target[k]=found->second;
        }return true;
    }
    static std::shared_ptr<const TextureAsset> resolve(void* raw,std::uint32_t address){
        auto& f=*static_cast<Fixture*>(raw);++f.resolves;
        if(f.replace_at&&f.resolves==f.replace_at){
            auto previous=f.owners.at(t1);
            auto replacement=std::make_shared<TextureAsset>(*previous);
            replacement->serial=previous->serial+1;
            f.owners[t1]=replacement;
        }
        const auto found=f.owners.find(address);
        return found==f.owners.end()?nullptr:found->second;
    }
    static std::shared_ptr<const TextureAsset> texture(std::uint32_t ptr,std::uint64_t serial,
        const char* name,std::shared_ptr<const std::vector<std::uint8_t>> bytes){
        auto result=std::make_shared<TextureAsset>();result->texture=ptr;result->serial=serial;
        result->logical_name=name;result->bytes=std::move(bytes);return result;
    }
    Fixture(){
        model->info.database=db;model->info.serial=7;model->bytes={1,2,3};
        model->info.byte_count=3;std::strcpy(model->info.name,"ship.sod");
        source.model=model;source.authored=authored;source.data=data;
        authored->nodes.resize(3);data->nodes.resize(3);
        const std::uint32_t addresses[]{root,mesh1,mesh2};
        const std::uint32_t name_pointers[]{0x5000,0x5040,0x5080};
        const char* names[]{"root","mesh-a","mesh-b"};
        for(unsigned k=0;k<3;++k){auto& node=data->nodes[k];auto& pose=authored->nodes[k];
            node.address=addresses[k];node.vtable=0x9300+0x20*k;
            node.type_function=k?profile.mesh_type_function:0x9200;
            node.name_pointer=name_pointers[k];node.name=names[k];
            node.parent=k?0:UINT32_MAX;node.parent_address=k?root:0;
            node.flags=0x80;pose.name=node.name;pose.parent=node.parent;pose.kind=k?1:0;
            put(node.address,node.vtable);put(node.vtable+4,node.type_function);
            put(node.address+4,db);put(node.address+8,node.name_pointer);str(node.name_pointer,names[k]);
        }
        data->nodes[0].first_child=1;data->nodes[0].child_address=mesh1;
        data->nodes[1].next_sibling=2;data->nodes[1].sibling_address=mesh2;
        data->root_address=root;
        for(const auto& node:data->nodes){put(node.address+0x18,node.parent_address);
            put(node.address+0x1c,node.child_address);put(node.address+0x20,node.sibling_address);}
        put(db,profile.database_vtable);put(db+0x38,std::uint32_t{0x3500});str(0x3500,"ship.sod");
        put(db+0x3c,root);put(db+0x2c,std::uint32_t{0});
        put(db+0x30,std::uint32_t{0});put(db+0x34,std::uint32_t{0});
        auto bytes1=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>(1024*1024,1));
        auto bytes2=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{2,3});
        owners[t1]=texture(t1,11,"one.dds",bytes1);owners[t2]=texture(t2,12,"two.dds",bytes2);
        put(t1+8,std::uint32_t{0x7000});str(0x7000,"one.dds");
        put(t2+8,std::uint32_t{0x7100});str(0x7100,"two.dds");
        put(t1+0x18,std::uint32_t{0});put(t2+0x18,std::uint32_t{0});
        put(t1+0x2c,std::uint8_t{0});put(t2+0x2c,std::uint8_t{0});
        put(mesh1+0x114,vector1);put(mesh1+0x11c,std::uint32_t{3});
        put(vector1,t1);put(vector1+4,std::uint32_t{0});put(vector1+8,t2);
        put(mesh1+0x13c,t2);
        put(mesh2+0x114,vector2);put(mesh2+0x11c,std::uint32_t{1});
        put(vector2,t1);put(mesh2+0x13c,std::uint32_t{0});
        // Mutable render output is deliberately absent from the reader map.
    }
    bool capture(std::shared_ptr<const ModelMaterialBaseline>& out,std::string& error,
                 MaterialBaselineReadProfile p=profile){
        return capture_model_material_baseline(source,p,this,read,this,resolve,out,error);
    }
};
void rejected(Fixture& f,const char* why,MaterialBaselineReadProfile p=profile){
    auto sentinel=std::make_shared<ModelMaterialBaseline>();
    std::shared_ptr<const ModelMaterialBaseline> output=sentinel;std::string error;
    need(!f.capture(output,error,p)&&!error.empty()&&output==sentinel,why);
}
}
int main(){
    Fixture f;std::shared_ptr<const ModelMaterialBaseline> output;std::string error;
    const bool first_capture=f.capture(output,error);
    need(first_capture,error.c_str());
    need(error.empty()&&output->model==f.model&&
         output->authored==f.authored&&output->nodes==f.data&&output->data->meshes.size()==2&&
         output->data->meshes[0].node_index==1&&output->data->meshes[0].slot_vector==vector1&&
         output->data->meshes[0].slots.size()==3&&output->data->meshes[0].slots[0]==f.owners[t1]&&
         !output->data->meshes[0].slots[1]&&output->data->meshes[0].slots[2]==f.owners[t2]&&
         output->data->meshes[0].borg_alternate==f.owners[t2]&&
         output->data->meshes[1].slots[0]==f.owners[t1]&&
         !output->data->meshes[1].borg_alternate,
         "raw mesh slots, null slot and Borg alternate retain exact texture owners");
    Fixture no_mesh;no_mesh.authored->nodes[1].kind=0;no_mesh.authored->nodes[2].kind=0;
    for(const auto index:{1u,2u}){
        no_mesh.data->nodes[index].type_function=0x9200;
        no_mesh.put(no_mesh.data->nodes[index].vtable+4,std::uint32_t{0x9200});
    }
    need(no_mesh.capture(output,error)&&output->data->meshes.empty(),
         "legitimate no-mesh template yields explicit empty baseline");
    Fixture wrong_type;wrong_type.put(mesh1+0x114,vector1);
    wrong_type.data->nodes[1].type_function=0x9200;
    wrong_type.put(wrong_type.data->nodes[1].vtable+4,std::uint32_t{0x9200});
    rejected(wrong_type,"authored mesh must have pinned mesh type function");
    Fixture bad_db;bad_db.put(db+0x3c,std::uint32_t{0});rejected(bad_db,"wrong database root rejected");
    Fixture bad_name;bad_name.str(0x3500,"other.sod");rejected(bad_name,"changed database name rejected");
    Fixture bad_link;bad_link.put(mesh1+0x20,std::uint32_t{0});rejected(bad_link,"changed node topology rejected");
    Fixture bad_slot;bad_slot.put(mesh1+0x11c,std::uint32_t{9});rejected(bad_slot,"slot count over eight rejected");
    Fixture overflow;overflow.put(mesh1+0x114,std::uint32_t{UINT32_MAX-1});
    rejected(overflow,"full slot span overflow rejected");
    Fixture bad_alt;bad_alt.put(mesh1+0x13c,std::uint32_t{0xdead});
    rejected(bad_alt,"unowned alternate texture rejected");
    Fixture bad_flags;bad_flags.put(t1+0x18,std::uint32_t{1});
    rejected(bad_flags,"runtime texture flags must agree with immutable asset");
    Fixture bad_key;bad_key.put(t1+0x2c,std::uint8_t{1});
    rejected(bad_key,"runtime colour key must agree with immutable asset");
    Fixture raw_metadata;
    auto flagged=std::make_shared<TextureAsset>(*raw_metadata.owners[t1]);
    flagged->flags=7;flagged->colour_key=true;raw_metadata.owners[t1]=flagged;
    raw_metadata.put(t1+0x18,std::uint32_t{7});raw_metadata.put(t1+0x2c,std::uint8_t{1});
    std::shared_ptr<const ModelMaterialBaseline> flagged_baseline;
    need(raw_metadata.capture(flagged_baseline,error)&&
         flagged_baseline->data->meshes[0].slots[0]==flagged&&
         flagged_baseline->data->meshes[0].slots[0]->flags==7&&
         flagged_baseline->data->meshes[0].slots[0]->colour_key,
         "raw flagged and keyed texture metadata remains captured when native bytes agree");
    Fixture bad_texture_name;bad_texture_name.str(0x7000,"other.dds");
    rejected(bad_texture_name,"runtime texture name must agree with immutable owner");
    Fixture changed;changed.trigger=db;changed.trigger_at=2;
    changed.on_read=[](Fixture& x){x.put(vector1+4,t1);};
    rejected(changed,"slot pointer change between complete passes rejected atomically");
    Fixture changed_alt;changed_alt.trigger=db;changed_alt.trigger_at=2;
    changed_alt.on_read=[](Fixture& x){x.put(mesh1+0x13c,t1);};
    rejected(changed_alt,"Borg alternate change between complete passes rejected atomically");
    Fixture changed_header;changed_header.trigger=db;changed_header.trigger_at=2;
    changed_header.on_read=[](Fixture& x){x.put(db+0x38,std::uint32_t{0x3600});x.str(0x3600,"ship.sod");};
    rejected(changed_header,"database name-pointer change between passes rejected");
    Fixture replaced;replaced.replace_at=4;
    rejected(replaced,"texture owner replacement across slots/passes rejected");
    Fixture throwing;throwing.throwing=true;rejected(throwing,"throwing read preserves output");
    Fixture unknown;unknown.reads=0;rejected(unknown,"missing profile rejected before reads",{0,profile.mesh_type_function});
    need(unknown.reads==0,"invalid profile performs no memory read");
    Fixture invalid_model;invalid_model.model->info.serial=0;
    invalid_model.reads=0;rejected(invalid_model,"missing model version rejected");
    need(invalid_model.reads==0,"invalid model performs no memory read");

    Fixture cache_fixture;std::shared_ptr<const ModelMaterialBaseline> captured;
    need(cache_fixture.capture(captured,error),"cache source captured");
    ModelMaterialBaselineCache cache;
    need(cache.publish(captured,error)&&cache.size()==1&&cache.bytes()>1024*1024,
         "cache retains copied metadata and unique texture bytes");
    const auto first_cost=cache.bytes();
    std::shared_ptr<const ModelMaterialBaseline> found;
    need(cache.find(cache_fixture.model,found,error)&&found!=captured&&
         found->model==captured->model&&found->authored==captured->authored&&
         found->nodes==captured->nodes&&found->data==captured->data,
         "cache returns a fresh wrapper with exact owner tuple");
    need(cache.publish(found,error)&&cache.bytes()==first_cost,"same tuple publication is idempotent");
    auto different=std::make_shared<ModelMaterialBaseline>(*captured);
    different->data=std::make_shared<ModelMaterialBaselineData>(*captured->data);
    need(!cache.publish(different,error)&&cache.size()==1,"live tuple replacement rejected");
    auto alias_model=std::make_shared<ModelAsset>(*cache_fixture.model);
    different->model=alias_model;
    need(!cache.publish(different,error),"same serial on foreign model owner rejected");
    auto alias_lookup=std::shared_ptr<const ModelAsset>(alias_model);
    const auto retained=found;
    need(!cache.find(alias_lookup,found,error)&&found==retained,
         "serial alias lookup preserves previous output");
    auto second=std::make_shared<ModelMaterialBaseline>(*captured);
    auto second_model=std::make_shared<ModelAsset>(*cache_fixture.model);second_model->info.serial=8;
    second->model=second_model;
    need(cache.publish(second,error)&&cache.size()==2&&
         cache.bytes()>first_cost&&cache.bytes()<first_cost+1024*1024,
         "second model sharing texture byte owners counts only extra metadata");
    ModelMaterialBaselineCache small(1,first_cost-1);
    need(!small.publish(captured,error)&&small.size()==0,"byte budget refuses oversized snapshot atomically");
    ModelMaterialBaselineCache one(1);
    need(one.publish(captured,error)&&!one.publish(second,error)&&one.size()==1,
         "entry budget refuses second live tuple");
    ModelMaterialBaselineCache weak_cache;
    {Fixture transient;std::shared_ptr<const ModelMaterialBaseline> temporary;
        need(transient.capture(temporary,error)&&weak_cache.publish(temporary,error),
             "transient owned baseline published");
    }
    need(weak_cache.size()==0&&weak_cache.bytes()==0,
         "expired weak model prunes copied metadata and texture byte budget");
    cache.clear();need(cache.size()==0&&cache.bytes()==0&&retained->model==cache_fixture.model,
                       "clear revokes lookup while retained result stays alive");
    std::puts("PASS native11 completed-load model material baseline and cache");
}
