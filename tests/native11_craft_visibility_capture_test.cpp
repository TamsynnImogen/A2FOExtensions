#include "../renderer/native11/craft_visibility_capture.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
constexpr std::uint32_t entity=0x1000,instance=0x2000,database=0x3000,
    object_class=0xa000,engine_pointer=0xb000,engine=0xb100,context=0xb200,
    ivtable=0xc000,dvtable=0xc100,other_database=0xd000;
constexpr std::array<std::uint32_t,5> class_offsets{{0x40c,0x418,0x410,0x41c,0x414}};
constexpr std::array<std::uint32_t,5> instance_offsets{{0x9c,0x9d,0x9e,0xa0,0x9f}};
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::set<std::uint32_t> unreadable;
    std::map<std::uint32_t,unsigned> reads;
    template<class T> void put(std::uint32_t at,const T& value){
        const auto* p=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t i=0;i<sizeof(value);++i)bytes[at+static_cast<std::uint32_t>(i)]=p[i];
    }
    void str(std::uint32_t at,const std::string& value){
        for(std::size_t i=0;i<value.size();++i)put(at+static_cast<std::uint32_t>(i),value[i]);
        put(at+static_cast<std::uint32_t>(value.size()),char{0});
    }
    void deny(std::uint32_t at,std::size_t count){for(std::size_t i=0;i<count;++i)unreadable.insert(at+static_cast<std::uint32_t>(i));}
    static bool read(void* raw,std::uint32_t at,void* out,std::size_t count){
        auto& m=*static_cast<Memory*>(raw);++m.reads[at];
        auto* dst=static_cast<std::uint8_t*>(out);
        for(std::size_t i=0;i<count;++i){const auto addr=at+static_cast<std::uint32_t>(i);
            const auto found=m.bytes.find(addr);
            if(addr<at||m.unreadable.count(addr)||found==m.bytes.end())return false;
            dst[i]=found->second;
        }
        return true;
    }
};
struct Classes {
    std::shared_ptr<const LightClassRecord> record;
    unsigned calls=0;
    std::function<void()> at_second_pass;
    bool replace_owner=false;
    static std::shared_ptr<const LightClassRecord> pin(void* raw,std::uint64_t id){
        auto& c=*static_cast<Classes*>(raw);++c.calls;
        if(c.calls==3){if(c.at_second_pass)c.at_second_pass();
            if(c.replace_owner)c.record=std::make_shared<LightClassRecord>(*c.record);}
        return c.record&&c.record->class_id==id?c.record:nullptr;
    }
};
struct Fixture {
    Memory memory;Classes classes;Object object;WorldMember member;
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> data=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<ModelNodeTemplate> baseline=std::make_shared<ModelNodeTemplate>();
    CraftVisibilityReadProfile profile{ivtable,dvtable,engine_pointer,context};
    Fixture(){
        model->info.database=database;model->info.serial=7;model->bytes={1};
        model->info.byte_count=1;std::strcpy(model->info.name,"craft.sod");
        baseline->model=model;baseline->authored=authored;baseline->data=data;
        object.entity=entity;object.instance=instance;object.instance_vtable=ivtable;
        object.database=database;object.database_vtable=dvtable;object.model_serial=7;
        object.model_identity=ModelIdentity::plain_database_name;std::strcpy(object.model_name,"craft.sod");
        member={entity,instance,object_class,5};
        auto owner=std::make_shared<LightClassRecord>();owner->class_id=object_class;owner->version=5;
        classes.record=owner;
        memory.put(entity+4,instance);memory.put(entity+0x40,object_class);
        memory.put(instance,ivtable);memory.put(instance+0x80,database);
        memory.put(instance+0x98,object_class);memory.put(database,dvtable);
        memory.put(database+0x38,std::uint32_t{0xe000});memory.str(0xe000,"craft.sod");
        memory.put(engine_pointer,engine);memory.put(engine+0x104,context);
        memory.put(context+0x40,std::uint32_t{1});
        memory.put(instance+0x90,std::uint8_t{0});memory.put(object_class+0x1dc,database);
        data->hardpoints=1;data->damage=2;data->lights=5;
        const std::array<const char*,12> names{{"root","hardpoints","damage","d0","d1","lights",
            "crew","life","weapons","engines","sensors","assimilation"}};
        data->nodes.resize(names.size());authored->nodes.resize(names.size());
        for(std::size_t i=0;i<names.size();++i){
            auto& n=data->nodes[i];auto& a=authored->nodes[i];
            n.address=0x4000+static_cast<std::uint32_t>(i)*0x100;
            n.vtable=0x7000+static_cast<std::uint32_t>(i)*0x20;
            n.type_function=0x8000+static_cast<std::uint32_t>(i)*0x10;
            n.name_pointer=0x9000+static_cast<std::uint32_t>(i)*0x40;
            n.name=names[i];a.name=n.name;
            n.parent=i==0?UINT32_MAX:(i==3||i==4?2:0);a.parent=n.parent;
            n.flags=i==3||i==4?0x81u:0x80u;
            memory.put(n.address,n.vtable);memory.put(n.vtable+4,n.type_function);
            memory.put(n.address+4,database);memory.put(n.address+8,n.name_pointer);
            memory.str(n.name_pointer,n.name);
            // Mutable live flags, scale, and matrices are deliberately unavailable.
            memory.deny(n.address+0x24,0x38);
            memory.deny(n.address+0xbc,4);
        }
        auto link=[&](unsigned parent,unsigned child,unsigned sibling){
            auto& n=data->nodes[child];n.parent=parent;n.parent_address=data->nodes[parent].address;
            n.next_sibling=sibling==UINT32_MAX?UINT32_MAX:sibling;
            n.sibling_address=sibling==UINT32_MAX?0:data->nodes[sibling].address;
            authored->nodes[child].parent=parent;
        };
        data->nodes[0].first_child=1;data->nodes[0].child_address=data->nodes[1].address;
        link(0,1,2);link(0,2,5);link(0,5,6);
        for(unsigned i=6;i<11;++i)link(0,i,i+1);
        link(0,11,UINT32_MAX);
        data->nodes[2].first_child=3;data->nodes[2].child_address=data->nodes[3].address;
        link(2,3,4);link(2,4,UINT32_MAX);
        data->root_address=data->nodes[0].address;
        memory.put(database+0x3c,data->root_address);
        memory.put(database+0x2c,data->nodes[1].address);
        memory.put(database+0x30,data->nodes[2].address);
        memory.put(database+0x34,data->nodes[5].address);
        for(const auto& n:data->nodes){
            memory.put(n.address+0x18,n.parent_address);
            memory.put(n.address+0x1c,n.child_address);
            memory.put(n.address+0x20,n.sibling_address);
        }
        memory.put(object_class+0x408,data->nodes[2].address);
        memory.put(object_class+0x420,data->nodes[11].address);
        for(unsigned i=0;i<5;++i){memory.put(object_class+class_offsets[i],data->nodes[6+i].address);
            memory.put(instance+instance_offsets[i],static_cast<std::uint8_t>(i==0||i==3?1u:0u));}
        memory.put(instance+0xa1,std::uint8_t{0});memory.put(instance+0xa4,.7f);
    }
    bool capture(CapturedCraftVisibility& out,std::string& error){
        return capture_craft_visibility(object,member,classes.record,baseline,profile,
            &memory,Memory::read,&classes,Classes::pin,out,error);
    }
};
void reject(const char* why,const std::function<void(Fixture&)>& edit){
    Fixture f;edit(f);CapturedCraftVisibility out;
    out.entity=77;out.instance=88;out.quality=99;out.input.root=123;
    out.baseline=std::make_shared<ModelNodeTemplate>();
    const auto sentinel=out.baseline;std::string error="stale";
    need(!f.capture(out,error),why);
    need(out.entity==77&&out.instance==88&&out.quality==99&&out.input.root==123&&
         out.baseline==sentinel&&error!="stale"&&!error.empty(),"failure preserves output");
}
void cases(){
    Fixture f;CapturedCraftVisibility captured;std::string error="stale";
    need(f.capture(captured,error)&&error.empty(),"primary capture");
    need(captured.baseline==f.baseline&&captured.object_class==f.classes.record&&
         captured.entity==entity&&captured.instance==instance&&captured.engine==engine&&
         captured.render_context==context&&captured.quality==1,"identity and owners retained");
    const auto& in=captured.input;
    need(in.nodes.size()==12&&in.root==0&&in.primary_database&&in.database_damage==2&&
         in.damage_child==3&&in.base_group==5&&in.assimilation_group==11&&
         in.system_groups==std::array<std::uint32_t,5>{{6,7,8,9,10}}&&
         in.system_hidden==std::array<bool,5>{{true,false,false,true,false}}&&
         !in.instance_hidden&&in.context_enabled&&in.hull_fraction==.7f&&
         in.nodes[3].flags==0x81,"fresh role mapping and immutable template flags");
    need(f.memory.reads[f.data->nodes[0].address+0xbc]==0&&
         f.memory.reads[f.data->nodes[0].address+0x24]==0,
         "mutable live flags and matrices were never read");
    CraftVisibilityResult evaluated;
    need(evaluate_craft_visibility(in,evaluated,error)&&error.empty()&&
         evaluated.flags[2]==0x80&&evaluated.flags[3]==0x80&&evaluated.flags[4]==0x80&&
         evaluated.flags[6]==0x82&&evaluated.flags[9]==0x82&&
         evaluated.flags[11]==0x83&&evaluated.flags[5]==0x80&&
         evaluated.visible[3]&&evaluated.visible[4]&&!evaluated.visible[11],
         "pure visibility evaluation follows native role order");
    // A second entity shares the immutable model/class but has independent live flags.
    Fixture second;second.baseline=f.baseline;second.classes.record=f.classes.record;
    const auto other_entity=entity+0x200,other_instance=instance+0x200;
    std::vector<std::pair<std::uint32_t,std::uint8_t>> instance_bytes;
    for(const auto& byte:second.memory.bytes)
        if(byte.first>=instance&&byte.first<instance+0x100)instance_bytes.push_back(byte);
    for(const auto& byte:instance_bytes)second.memory.bytes[byte.first+0x200]=byte.second;
    second.object.entity=other_entity;second.object.instance=other_instance;
    second.member.entity=other_entity;second.member.instance=other_instance;
    second.memory.put(other_entity+4,other_instance);
    second.memory.put(other_entity+0x40,object_class);
    second.memory.put(other_instance+0x90,std::uint8_t{1});
    second.memory.put(other_instance+0x9c,std::uint8_t{0});
    second.memory.put(other_instance+0xa1,std::uint8_t{1});
    second.memory.put(other_instance+0xa4,.2f);
    CapturedCraftVisibility other;need(second.capture(other,error)&&
         other.entity==other_entity&&other.instance==other_instance&&
         other.baseline==captured.baseline&&other.object_class==captured.object_class&&
         other.input.instance_hidden&&other.input.assimilated&&other.input.hull_fraction==.2f&&
         !other.input.system_hidden[0]&&in.hull_fraction==.7f&&in.system_hidden[0],
         "separate captures keep instance inputs independent");
    need(evaluate_craft_visibility(other.input,evaluated,error)&&
         evaluated.flags[5]==0x83&&evaluated.flags[11]==0x80,
         "second instance evaluation differs");
    // Nonprimary class database must never read primary-only class roles or instance fields.
    Fixture alt;alt.memory.put(object_class+0x1dc,other_database);
    alt.memory.put(instance+0xa4,-1.f);
    for(unsigned i=0;i<5;++i){alt.memory.deny(object_class+class_offsets[i],4);
        alt.memory.deny(instance+instance_offsets[i],1);}
    alt.memory.deny(object_class+0x408,4);alt.memory.deny(object_class+0x420,4);
    alt.memory.deny(instance+0xa1,1);alt.memory.deny(instance+0xa4,4);
    CapturedCraftVisibility alternate;need(alt.capture(alternate,error)&&
         !alternate.input.primary_database&&alternate.input.damage_child==UINT32_MAX&&
         alternate.input.assimilation_group==UINT32_MAX&&alternate.input.hull_fraction==1.f,
         "alternate database skips primary-only reads");
    need(evaluate_craft_visibility(alternate.input,evaluated,error)&&
         evaluated.flags[2]==0x83&&evaluated.flags[5]==0x80,
         "alternate database uses damage and base roles");
    reject("class identity",[](Fixture& x){x.member.object_class++;});
    reject("class version",[](Fixture& x){x.member.class_version++;});
    reject("world membership",[](Fixture& x){x.member.instance++;});
    reject("class pin owner changes",[](Fixture& x){x.classes.replace_owner=true;});
    reject("instance vtable",[](Fixture& x){x.memory.put(instance,std::uint32_t{1});});
    reject("database vtable",[](Fixture& x){x.memory.put(database,std::uint32_t{1});});
    reject("database role",[](Fixture& x){x.memory.put(database+0x30,std::uint32_t{1});});
    reject("class role outside tree",[](Fixture& x){x.memory.put(object_class+0x420,std::uint32_t{1});});
    reject("invalid hidden flag",[](Fixture& x){x.memory.put(instance+0x90,std::uint8_t{2});});
    reject("invalid system flag",[](Fixture& x){x.memory.put(instance+0x9c,std::uint8_t{2});});
    reject("invalid assimilation flag",[](Fixture& x){x.memory.put(instance+0xa1,std::uint8_t{2});});
    reject("nonfinite hull",[](Fixture& x){x.memory.put(instance+0xa4,NAN);});
    reject("unreadable quality",[](Fixture& x){x.memory.deny(context+0x40,4);});
    reject("unreadable node link",[](Fixture& x){x.memory.deny(x.data->nodes[4].address+0x20,4);});
    reject("invalid baseline count",[](Fixture& x){x.authored->nodes.pop_back();});
    reject("invalid baseline index",[](Fixture& x){x.data->nodes[2].first_child=99;});
    reject("overflowing engine pointer",[](Fixture& x){x.profile.engine_pointer=UINT32_MAX;});
    reject("baseline exceeds node budget",[](Fixture& x){x.data->nodes.resize(4097);});
    reject("negative hull is outside evaluator admission",[](Fixture& x){x.memory.put(instance+0xa4,-.1f);});
    const std::array<std::pair<const char*,std::function<void(Fixture&)>>,17> changes{{
        {"entity",[](Fixture& x){x.memory.put(entity+4,std::uint32_t{1});}},
        {"instance",[](Fixture& x){x.memory.put(instance+0x80,std::uint32_t{1});}},
        {"class",[](Fixture& x){x.memory.put(instance+0x98,std::uint32_t{1});}},
        {"database",[](Fixture& x){x.memory.put(database+0x3c,std::uint32_t{1});}},
        {"context",[](Fixture& x){x.memory.put(engine+0x104,std::uint32_t{1});}},
        {"quality",[](Fixture& x){x.memory.put(context+0x40,std::uint32_t{0});}},
        {"tree link",[](Fixture& x){x.memory.put(x.data->nodes[1].address+0x20,std::uint32_t{1});}},
        {"name pointer",[](Fixture& x){x.memory.put(x.data->nodes[1].address+8,std::uint32_t{0});}},
        {"name byte",[](Fixture& x){x.memory.put(x.data->nodes[1].name_pointer,char{'X'});}},
        {"database name pointer",[](Fixture& x){x.memory.put(database+0x38,std::uint32_t{0});}},
        {"database name byte",[](Fixture& x){x.memory.put(0xe000,char{'X'});}},
        {"class role",[](Fixture& x){x.memory.put(object_class+0x40c,x.data->nodes[7].address);}},
        {"system byte",[](Fixture& x){x.memory.put(instance+0x9c,std::uint8_t{0});}},
        {"hidden byte",[](Fixture& x){x.memory.put(instance+0x90,std::uint8_t{1});}},
        {"assimilation byte",[](Fixture& x){x.memory.put(instance+0xa1,std::uint8_t{1});}},
        {"hull below",[](Fixture& x){x.memory.put(instance+0xa4,-.1f);}},
        {"hull above",[](Fixture& x){x.memory.put(instance+0xa4,1.1f);}}
    }};
    for(const auto& change:changes)reject(change.first,[&](Fixture& x){
        x.classes.at_second_pass=[&x,&change]{change.second(x);};
    });
    reject("hull +0 to -0 changes between passes",[](Fixture& x){
        x.memory.put(instance+0xa4,0.f);
        x.classes.at_second_pass=[&x]{x.memory.put(instance+0xa4,-0.f);};
    });
}
}
int main(){cases();std::puts("PASS native11 craft visibility capture");}
