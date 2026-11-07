#include "../renderer/native11/craft_borg_material_capture.hpp"
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
constexpr std::uint32_t db=0x3000,root=0x4000,a=0x4200,b=0x4400,c=0x4600;
constexpr std::uint32_t klass=0xa000,table=0xd000,t1=0x6000,t2=0x6100;
constexpr MaterialBaselineReadProfile profile{0x9200,0x9300};
const std::array<std::uint32_t,4> node_addresses{{root,a,b,c}};
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Fixture {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> textures;
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> nodes=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<LightClassRecord> object_class=std::make_shared<LightClassRecord>();
    std::shared_ptr<const ModelMaterialBaseline> baseline;
    std::function<void(Fixture&)> on_read;
    std::uint32_t trigger=0;unsigned trigger_at=0,trigger_count=0,reads=0;
    unsigned forbidden_reads=0,texture_pins=0,replace_texture_at=0,class_pins=0,replace_class_at=0;
    bool forbid_slots=false,throw_read=false,retire_model=false,foreign_model=false;
    bool retire_class=false,retire_texture=false;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=source[k];
    }
    void str(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
    static bool read(void* raw,std::uint32_t address,void* out,std::size_t size){
        auto& f=*static_cast<Fixture*>(raw);++f.reads;
        if(f.throw_read)throw std::runtime_error("synthetic read");
        if(address==f.trigger&&++f.trigger_count==f.trigger_at&&f.on_read)f.on_read(f);
        if(f.forbid_slots){
            for(const auto node:{a,b,c})
                if(address==node+0x114||address==node+0x11c)++f.forbidden_reads;
            for(const auto vector:{0x8000u,0x8100u,0x8200u})
                if(address>=vector&&address<vector+16)++f.forbidden_reads;
        }
        auto* target=static_cast<std::uint8_t*>(out);
        for(std::size_t k=0;k<size;++k){
            if(k>UINT32_MAX-address)return false;
            const auto found=f.bytes.find(address+static_cast<std::uint32_t>(k));
            if(found==f.bytes.end())return false;
            target[k]=found->second;
        }return true;
    }
    static std::shared_ptr<const ModelAsset> pin_model(void* raw,std::uint32_t database,std::uint64_t serial){
        auto& f=*static_cast<Fixture*>(raw);
        if(f.retire_model||database!=db||serial!=f.model->info.serial)return nullptr;
        return f.foreign_model?std::make_shared<ModelAsset>(*f.model):f.model;
    }
    static std::shared_ptr<const LightClassRecord> pin_class(void* raw,std::uint64_t id){
        auto& f=*static_cast<Fixture*>(raw);++f.class_pins;
        if(f.replace_class_at&&f.class_pins==f.replace_class_at)
            f.object_class=std::make_shared<LightClassRecord>(*f.object_class);
        return !f.retire_class&&id==klass?f.object_class:nullptr;
    }
    static std::shared_ptr<const TextureAsset> resolve(void* raw,std::uint32_t ptr){
        auto& f=*static_cast<Fixture*>(raw);++f.texture_pins;
        if(f.replace_texture_at&&f.texture_pins==f.replace_texture_at){
            auto changed=std::make_shared<TextureAsset>(*f.textures.at(t1));
            ++changed->serial;f.textures[t1]=changed;
        }
        const auto found=f.textures.find(ptr);
        return f.retire_texture||found==f.textures.end()?nullptr:found->second;
    }
    static std::shared_ptr<const TextureAsset> texture(std::uint32_t ptr,
        std::uint64_t serial,const char* name){
        auto result=std::make_shared<TextureAsset>();result->texture=ptr;result->serial=serial;
        result->logical_name=name;
        result->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{1,2,3});
        return result;
    }
    Fixture(){
        model->info.database=db;model->info.serial=7;model->bytes={1,2,3};
        model->info.byte_count=3;std::strcpy(model->info.name,"ship.sod");
        object_class->class_id=klass;object_class->version=5;
        authored->nodes.resize(4);nodes->nodes.resize(4);
        const char* names[]{"root","a","b","c"};
        for(unsigned k=0;k<4;++k){auto& n=nodes->nodes[k];auto& pose=authored->nodes[k];
            n.address=node_addresses[k];n.vtable=0x9000+0x20*k;
            n.type_function=k?profile.mesh_type_function:0x9400;
            n.name_pointer=0x5000+0x40*k;n.name=names[k];
            n.parent=k==0?UINT32_MAX:k==3?1:0;
            n.parent_address=k==0?0:k==3?a:root;
            pose.name=n.name;pose.parent=n.parent;pose.kind=k?1:0;
            put(n.address,n.vtable);put(n.vtable+4,n.type_function);
            put(n.address+4,db);put(n.address+8,n.name_pointer);str(n.name_pointer,names[k]);
        }
        nodes->nodes[0].first_child=1;nodes->nodes[0].child_address=a;
        nodes->nodes[1].first_child=3;nodes->nodes[1].child_address=c;
        nodes->nodes[1].next_sibling=2;nodes->nodes[1].sibling_address=b;
        nodes->root_address=root;
        for(const auto& n:nodes->nodes){put(n.address+0x18,n.parent_address);
            put(n.address+0x1c,n.child_address);put(n.address+0x20,n.sibling_address);}
        put(db,profile.database_vtable);put(db+0x38,std::uint32_t{0x3500});
        str(0x3500,"ship.sod");put(db+0x3c,root);
        for(const auto offset:{0x2cu,0x30u,0x34u})put(db+offset,std::uint32_t{0});
        textures[t1]=texture(t1,11,"one.dds");textures[t2]=texture(t2,12,"borg.dds");
        put(t1+8,std::uint32_t{0x7000});str(0x7000,"one.dds");
        put(t2+8,std::uint32_t{0x7100});str(0x7100,"borg.dds");
        put(t1+0x18,std::uint32_t{0});put(t2+0x18,std::uint32_t{0});
        put(t1+0x2c,std::uint8_t{0});put(t2+0x2c,std::uint8_t{0});
        const std::uint32_t meshes[]{a,b,c};const std::uint32_t vectors[]{0x8000,0x8100,0x8200};
        for(unsigned k=0;k<3;++k){put(meshes[k]+0x114,vectors[k]);
            put(meshes[k]+0x11c,std::uint32_t{4});put(meshes[k]+0x13c,t2);
            put(vectors[k],k==1?0:t1);
            for(unsigned slot=1;slot<4;++slot)put(vectors[k]+4*slot,std::uint32_t{0});
        }
        put(klass,std::uint32_t{0xb000});put(klass+0x1d8,db);
        put(klass+0x424,std::int32_t{3});put(klass+0x428,table);
        put(table,a);put(table+4,t1);put(table+8,b);put(table+12,std::uint32_t{0});
        put(table+16,c);put(table+20,t1);
        ModelNodeTemplate source{model,authored,nodes};std::string error;
        need(capture_model_material_baseline(source,profile,this,read,this,resolve,baseline,error),
             "fixture completed-load material baseline captured");
        reads=0;texture_pins=class_pins=0;forbid_slots=true;
    }
    bool capture(CapturedCraftBorgTable& output,std::string& error,
                 MaterialBaselineReadProfile p=profile){
        return capture_craft_borg_table(object_class,baseline,p,this,read,this,pin_model,
                                        this,pin_class,this,resolve,output,error);
    }
};
void rejected(Fixture& f,const char* why,MaterialBaselineReadProfile p=profile){
    CapturedCraftBorgTable output;output.class_vtable=0xdead;output.table_pointer=0xbeef;
    output.rows.push_back({77,1,2,4});auto saved=output.rows;std::string error;
    need(!f.capture(output,error,p)&&!error.empty()&&output.class_vtable==0xdead&&
         output.table_pointer==0xbeef&&output.rows.size()==saved.size()&&
         output.rows[0].node_index==saved[0].node_index,why);
}
}
int main(){
    Fixture f;CapturedCraftBorgTable output;std::string error;
    need(f.capture(output,error)&&error.empty()&&output.object_class==f.object_class&&
         output.baseline==f.baseline&&output.class_vtable==0xb000&&output.table_pointer==table&&
         output.rows.size()==3&&output.rows[0].node_index==1&&
         output.rows[1].node_index==2&&output.rows[1].original_texture==0&&
         output.rows[2].node_index==3&&output.rows[2].original_texture==t1&&
         output.rows[0].borg_texture==t2&&f.forbidden_reads==0,
         "sibling-before-child rows retain original slot zero, null original and alternate without live slot reads");
    f.put(0x8000,std::uint32_t{0xdead});f.put(a+0x114,std::uint32_t{0xdead});
    need(f.capture(output,error)&&f.forbidden_reads==0&&output.rows[0].original_texture==t1,
         "mutated live slot vector is never read or used as the completed-load original");
    Fixture empty;for(const auto node:{a,b,c})empty.put(node+0x13c,std::uint32_t{0});
    ModelNodeTemplate empty_source{empty.model,empty.authored,empty.nodes};
    need(capture_model_material_baseline(empty_source,profile,&empty,Fixture::read,&empty,Fixture::resolve,
         empty.baseline,error),"empty-alternate completed-load baseline recaptured");
    empty.put(klass+0x424,std::int32_t{0});empty.put(klass+0x428,std::uint32_t{0});
    need(empty.capture(output,error)&&output.rows.empty()&&output.table_pointer==0,
         "no eligible alternates give explicit empty class table");
    Fixture bad_class_db;bad_class_db.put(klass+0x1d8,std::uint32_t{0});
    rejected(bad_class_db,"class database mismatch rejected");
    Fixture negative;negative.put(klass+0x424,std::int32_t{-1});
    rejected(negative,"negative signed count rejected");
    Fixture too_many;too_many.put(klass+0x424,std::int32_t{4097});
    rejected(too_many,"class row count beyond bound rejected");
    Fixture no_table;no_table.put(klass+0x428,std::uint32_t{0});
    rejected(no_table,"missing nonempty class table rejected");
    Fixture overflow;overflow.put(klass+0x428,UINT32_MAX-3);
    rejected(overflow,"class table row full-span overflow rejected");
    Fixture zero_count_pointer;zero_count_pointer.put(klass+0x424,std::int32_t{0});
    rejected(zero_count_pointer,"zero count with nonnull class table rejected");
    Fixture reordered;reordered.put(table,b);reordered.put(table+8,a);
    rejected(reordered,"reordered native rows rejected");
    Fixture duplicate;duplicate.put(table+8,a);
    rejected(duplicate,"duplicate native row rejected");
    Fixture wrong_original;wrong_original.put(table+4,t2);
    rejected(wrong_original,"row original differs from completed-load slot zero");
    Fixture wrong_db;wrong_db.put(db+0x3c,std::uint32_t{0});
    rejected(wrong_db,"changed database root rejected");
    Fixture wrong_node;wrong_node.put(c+0x18,std::uint32_t{0});
    rejected(wrong_node,"changed native topology rejected");
    Fixture wrong_alt;wrong_alt.put(a+0x13c,std::uint32_t{0});
    rejected(wrong_alt,"changed live alternate rejected");
    Fixture second_pass;second_pass.trigger=db;second_pass.trigger_at=2;
    second_pass.on_read=[](Fixture& x){x.put(table+4,t2);};
    rejected(second_pass,"row mutation between complete passes rejected");
    Fixture header_pass;header_pass.trigger=db;header_pass.trigger_at=2;
    header_pass.on_read=[](Fixture& x){x.put(klass,std::uint32_t{0xb004});};
    rejected(header_pass,"class header mutation between passes rejected");
    Fixture model_retired;model_retired.retire_model=true;
    rejected(model_retired,"retired model pin rejected");
    Fixture model_replaced;model_replaced.foreign_model=true;
    rejected(model_replaced,"same-serial foreign model owner rejected");
    Fixture class_retired;class_retired.retire_class=true;
    rejected(class_retired,"retired class pin rejected");
    Fixture texture_retired;texture_retired.retire_texture=true;
    rejected(texture_retired,"retired texture pin rejected");
    Fixture class_replaced;class_replaced.replace_class_at=2;
    rejected(class_replaced,"class replacement during two-pass capture rejected");
    Fixture texture_replaced;texture_replaced.replace_texture_at=2;
    rejected(texture_replaced,"texture replacement during owner checks rejected");
    Fixture unreadable;unreadable.bytes.erase(table+20);
    rejected(unreadable,"late unreadable class row rejected atomically");
    Fixture throwing;throwing.throw_read=true;
    rejected(throwing,"throwing reader rejected atomically");
    Fixture bad_profile;bad_profile.reads=0;
    rejected(bad_profile,"missing mesh type profile rejected",{profile.database_vtable,0});
    need(bad_profile.reads==0,"invalid profile performs no native reads");
    Fixture bad_class_address;bad_class_address.object_class->class_id=UINT32_MAX-2;
    rejected(bad_class_address,"class header full-span overflow rejected");
    need(bad_class_address.reads==0,"invalid class address performs no native reads");
    std::puts("PASS native11 Craft Borg class table capture");
}
