#include "../renderer/native11/world_model_association.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace a2fo::native11;
constexpr std::uint32_t list=0x100,container=0x200,sentinel=0x300;
constexpr std::uint32_t n1=0x400,n2=0x420,n3=0x440;
constexpr std::uint32_t e1=0x500,e2=0x600,e3=0x700;
constexpr std::uint32_t i1=0x800,i3=0x900,d1=0xa00,d3=0xb00;
constexpr std::uint32_t name1=0xc00,name3=0xd00;
constexpr std::uint32_t craft_vtable=0x111100,db_vtable=0x222200;
const WorldModelReadProfile profile{list,craft_vtable,db_vtable};
const std::vector<WorldMember> roster{{e1,i1,0x5010,1},{e2,0,0x5020,2},{e3,i3,0x5030,3}};
const std::array<float,12> placement{{2,0,0,.25f,0,3,0,-.5f,0,0,4,1.5f}};
const std::array<float,3> scale{{.5f,1.25f,2.f}};

void need(bool okay,const char* message){if(!okay){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
struct Arena {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,std::shared_ptr<const ModelAsset>> models;
    std::function<void(Arena&)> second;
    unsigned list_reads=0,forbidden_reads=0;
    bool forbid_database=false,throw_read=false,throw_pin=false;
    template<class T> void put(std::uint32_t address,const T& value){
        const auto* src=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=src[k];
    }
    void put_name(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
    void erase(std::uint32_t address){bytes.erase(address);}
};
bool read(void* context,std::uint32_t address,void* output,std::size_t length){
    auto& a=*static_cast<Arena*>(context);
    if(a.throw_read)throw std::runtime_error("synthetic read failure");
    if(address==list&&length==4&&++a.list_reads==2&&a.second)a.second(a);
    if(a.forbid_database&&address==i1+0x80){++a.forbidden_reads;return false;}
    auto* dst=static_cast<std::uint8_t*>(output);
    for(std::size_t k=0;k<length;++k){
        if(k>UINT32_MAX-address)return false;
        const auto found=a.bytes.find(address+static_cast<std::uint32_t>(k));
        if(found==a.bytes.end())return false;
        dst[k]=found->second;
    }
    return true;
}
std::shared_ptr<const ModelAsset> pin(void* context,std::uint32_t database,const char* name){
    auto& a=*static_cast<Arena*>(context);
    if(a.throw_pin)throw std::runtime_error("synthetic model lookup failure");
    const auto found=a.models.find(database);
    return found!=a.models.end()&&found->second&&found->second->info.name==std::string(name)
        ?found->second:nullptr;
}
std::shared_ptr<const ModelAsset> model(std::uint32_t db,const char* name,std::uint64_t serial){
    auto result=std::make_shared<ModelAsset>();
    result->info.database=db;result->info.serial=serial;
    std::strcpy(result->info.name,name);result->bytes={1,2,3};
    result->info.byte_count=static_cast<std::uint32_t>(result->bytes.size());return result;
}
Arena valid(){
    Arena a;
    a.put(list,container);a.put(container+4,sentinel);
    a.put(sentinel,n1);a.put(sentinel+4,n3);
    a.put(n1,n2);a.put(n1+4,sentinel);a.put(n1+8,e1);
    a.put(n2,n3);a.put(n2+4,n1);a.put(n2+8,e2);
    a.put(n3,sentinel);a.put(n3+4,n2);a.put(n3+8,e3);
    a.put(e1+4,i1);a.put(e1+0x40,roster[0].object_class);
    a.put(e2+4,std::uint32_t(0));a.put(e2+0x40,roster[1].object_class);
    a.put(e3+4,i3);a.put(e3+0x40,roster[2].object_class);
    a.put(i1,craft_vtable);a.put(i1+0x80,d1);a.put(i1+0x44,placement);
    a.put(i1+0x74,std::uint8_t(1));a.put(i1+0x28,scale);
    a.put(i3,craft_vtable);a.put(i3+0x80,d3);a.put(i3+0x44,placement);
    a.put(i3+0x74,std::uint8_t(0));
    a.put(d1,db_vtable);a.put(d1+0x38,name1);a.put_name(name1,"first.sod");
    a.put(d3,db_vtable);a.put(d3+0x38,name3);a.put_name(name3,"outside.sod");
    a.models[d1]=model(d1,"first.sod",1);a.models[d3]=model(d3,"outside.sod",2);
    return a;
}
WorldModelAssociations marker(){
    WorldModelAssociations out;WorldModelAssociation value;
    value.member={0xdead,0,0xbeef,9};value.status=WorldModelStatus::model_unavailable;
    value.model=model(0xface,"retained.sod",99);out.members.push_back(std::move(value));return out;
}
void failed(Arena& a,const std::vector<WorldMember>& members=roster,
            WorldModelReadProfile p=profile,Read reader=read,PinWorldModel owner=pin){
    auto output=marker();const auto previous=output.members[0].model;std::string error;
    need(!capture_world_model_associations(p,members,&a,reader,&a,owner,output,error)&&
         !error.empty()&&output.members.size()==1&&output.members[0].member.entity==0xdead&&
         output.members[0].status==WorldModelStatus::model_unavailable&&
         output.members[0].model==previous,"failure preserves previous association output");
}
WorldModelAssociations captured(Arena& a){
    WorldModelAssociations output;std::string error;
    need(capture_world_model_associations(profile,roster,&a,read,&a,pin,output,error)&&
         error.empty()&&output.members.size()==roster.size(),"stable world association capture");
    return output;
}
void status(Arena a,WorldModelStatus expected){
    const auto output=captured(a);
    need(output.members[0].status==expected&&output.members[1].status==WorldModelStatus::no_instance,
         "explicit per-member association status");
}
}
int main(){
    auto a=valid();const auto result=captured(a);
    need(result.members[0].status==WorldModelStatus::bound_current_database&&
         result.members[0].model==a.models[d1]&&result.members[0].member.entity==e1&&
         result.members[1].status==WorldModelStatus::no_instance&&
         result.members[2].status==WorldModelStatus::bound_current_database&&
         result.members[2].model==a.models[d3]&&result.members[2].member.entity==e3,
         "ordered world roster includes null instance and outside-visible model");
    need(std::memcmp(result.members[0].native_placement.data(),placement.data(),sizeof(placement))==0&&
         std::memcmp(result.members[0].runtime_scale.data(),scale.data(),sizeof(scale))==0&&
         result.members[0].scale_active==1&&result.members[0].native_placement[0]==2.f&&
         result.members[2].scale_active==0&&result.members[2].runtime_scale[0]==1.f,
         "raw Matrix34 and runtime scale copied separately without loader scale");

    a=valid();a.put(i1,0x1234u);a.forbid_database=true;
    status(a,WorldModelStatus::unsupported_instance);
    captured(a);need(a.forbidden_reads==0,"unsupported instance never reads database offset 80");
    a=valid();a.put(i1+0x80,std::uint32_t(0));status(a,WorldModelStatus::no_database);
    a=valid();a.put(d1,0x1234u);status(a,WorldModelStatus::unsupported_database);
    a=valid();a.erase(name1);status(a,WorldModelStatus::unreadable);
    a=valid();auto degenerate=placement;degenerate[0]=0;degenerate[1]=0;degenerate[2]=0;
    a.put(i1+0x44,degenerate);status(a,WorldModelStatus::invalid_placement);
    a=valid();a.models.erase(d1);status(a,WorldModelStatus::model_unavailable);

    a=valid();auto extra=roster;extra.push_back({0x999,0,0x777,4});failed(a,extra);
    a=valid();auto missing=roster;missing.pop_back();failed(a,missing);
    a=valid();auto reordered=roster;std::swap(reordered[0],reordered[1]);failed(a,reordered);
    a=valid();auto duplicate=roster;duplicate[1]=duplicate[0];failed(a,duplicate);
    a=valid();a.put(n3,n1);failed(a);
    a=valid();a.put(n2+4,sentinel);failed(a);
    a=valid();a.second=[](Arena& x){auto changed=placement;changed[3]=3.f;x.put(i1+0x44,changed);};failed(a);
    a=valid();a.second=[](Arena& x){x.put(i1+0x80,d3);};failed(a);
    a=valid();a.second=[](Arena& x){x.put_name(name1,"other.sod");};failed(a);
    a=valid();a.second=[](Arena& x){x.models[d1]=model(d1,"first.sod",3);};failed(a);
    a=valid();a.throw_read=true;failed(a);
    a=valid();a.throw_pin=true;failed(a);
    a=valid();failed(a,roster,{0,craft_vtable,db_vtable});
    a=valid();failed(a,roster,{list,0,db_vtable});
    a=valid();auto invalid=roster;invalid[0].class_version=0;failed(a,invalid);
    a=valid();std::vector<WorldMember> over(4097,roster[0]);failed(a,over);
    a=valid();failed(a,roster,profile,nullptr,pin);
    a=valid();failed(a,roster,profile,read,nullptr);
    std::puts("PASS native11 world model association reader");
}
