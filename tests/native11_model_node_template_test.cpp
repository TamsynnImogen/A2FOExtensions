#include "../renderer/native11/model_node_template.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
using namespace a2fo::native11;
namespace {
constexpr NodeTemplateReadProfile profile{0x7000,0x7100,0x7110,0x7120,0x7130,0x7140,0x7150};
constexpr std::array<float,12> identity{1,0,0,0,1,0,0,0,1,0,0,0};
enum class Change {none,header,role,db_name_pointer,db_name_byte,
                   name_pointer,name_byte,type,owner,parent,child,sibling,
                   matrix,scale,flags,unreadable};
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    Change change=Change::none;
    unsigned passes=0;
    std::uint32_t unreadable=0;
    template<class T> void put(std::uint32_t address,T value){
        const auto* data=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t i=0;i<sizeof(T);++i)bytes[address+i]=data[i];
    }
    void name(std::uint32_t address,const char* value){
        for(std::size_t i=0;i<=std::strlen(value);++i)put(address+i,std::uint8_t(value[i]));
    }
    void node(std::uint32_t address,std::uint32_t vtable,std::uint32_t name_ptr,
              std::uint32_t parent,std::uint32_t child,std::uint32_t sibling,
              std::uint32_t flags){
        put(address,vtable);put(address+4,std::uint32_t(0x4000));put(address+8,name_ptr);
        put(address+0x18,parent);put(address+0x1c,child);put(address+0x20,sibling);
        for(std::size_t i=0;i<12;++i)put(address+0x24+4*i,identity[i]);
        put(address+0x54,1.0f);put(address+0xbc,flags);
    }
    void mutate(){
        switch(change){
        case Change::header:put(0x4000,std::uint32_t(0x7001));break;
        case Change::role:put(0x402c,std::uint32_t(0x5200));break;
        case Change::db_name_pointer:put(0x4038,std::uint32_t(0x9110));break;
        case Change::db_name_byte:put(0x9100,std::uint8_t('X'));break;
        case Change::name_pointer:put(0x5108,std::uint32_t(0x9110));break;
        case Change::name_byte:put(0x9000,std::uint8_t('X'));break;
        case Change::type:put(0x7124,std::uint32_t(0x7130));break;
        case Change::owner:put(0x5104,std::uint32_t(0x4001));break;
        case Change::parent:put(0x5118,std::uint32_t(0));break;
        case Change::child:put(0x511c,std::uint32_t(0));break;
        case Change::sibling:put(0x5120,std::uint32_t(0));break;
        case Change::matrix:put(0x5128,-0.0f);break; // +0 to -0: bitwise check
        case Change::scale:put(0x5154,2.0f);break;
        case Change::flags:put(0x51bc,std::uint32_t(9));break;
        case Change::unreadable:unreadable=0x51bc;break;
        case Change::none:break;
        }
    }
    static bool read(void* raw,std::uint32_t address,void* output,std::size_t length){
        auto& m=*static_cast<Memory*>(raw);
        if(address==0x4000&&++m.passes==2)m.mutate();
        if(address==m.unreadable||length>UINT32_MAX-address)return false;
        auto* dest=static_cast<std::uint8_t*>(output);
        for(std::size_t i=0;i<length;++i){
            auto found=m.bytes.find(address+static_cast<std::uint32_t>(i));
            if(found==m.bytes.end())return false;
            dest[i]=found->second;
        }
        return true;
    }
};
Memory fixture(){
    Memory m;
    m.put(0x4000,std::uint32_t(0x7000));m.put(0x403c,std::uint32_t(0x5000));
    m.put(0x4038,std::uint32_t(0x9100));
    m.name(0x9100,"ship.sod");m.name(0x9110,"ship.sod");
    m.put(0x402c,std::uint32_t(0x5100));m.put(0x4030,std::uint32_t(0x5200));
    m.put(0x4034,std::uint32_t(0x5300));
    for(auto [vt,type]:{std::pair{0x7100u,0x7100u},std::pair{0x7110u,0x7110u},
                        std::pair{0x7120u,0x7120u},std::pair{0x7130u,0x7130u},
                        std::pair{0x7140u,0x7140u},std::pair{0x7150u,0x7150u}})
        m.put(vt+4,type);
    m.name(0x9000,"mesh");m.name(0x9010,"root");m.name(0x9020,"lod");
    m.name(0x9030,"light");m.name(0x9040,"sprite");m.name(0x9050,"emitter");
    // Native sibling order differs from authored file order: lod, mesh, sprite, emitter.
    m.node(0x5000,0x7100,0x9010,0,0x5200,0,1);
    m.node(0x5200,0x7110,0x9020,0x5000,0,0x5100,2);
    m.node(0x5100,0x7120,0x9000,0x5000,0x5300,0x5400,3);
    m.node(0x5300,0x7130,0x9030,0x5100,0,0,4);
    m.node(0x5400,0x7140,0x9040,0x5000,0,0x5500,5);
    m.node(0x5500,0x7150,0x9050,0x5000,0,0,6);
    return m;
}
ModelPoseSnapshot model(){
    auto asset=std::make_shared<ModelAsset>();asset->info.database=0x4000;
    asset->info.serial=42;asset->bytes={1};asset->info.byte_count=1;
    std::strcpy(asset->info.name,"ship.sod");
    auto pose=std::make_shared<AuthoredPoseData>();
    for(auto [name,parent,kind]:{
        std::tuple{"mesh",1u,1u},std::tuple{"root",UINT32_MAX,0u},
        std::tuple{"lod",1u,0u},std::tuple{"light",0u,2u},
        std::tuple{"sprite",1u,3u},std::tuple{"emitter",1u,12u}}){
        AuthoredPoseNode node;node.name=name;node.parent=parent;node.kind=kind;node.local=identity;
        pose->nodes.push_back(std::move(node));
    }
    return {asset,pose};
}
void reject(Memory memory,const ModelPoseSnapshot& source,
            std::shared_ptr<const ModelNodeTemplate>& output){
    const auto previous=output;std::string error;
    assert(!capture_model_node_template(source,profile,&memory,Memory::read,output,error));
    assert(output==previous&&!error.empty());
}
std::pair<Memory,ModelPoseSnapshot> chain(std::size_t count){
    auto memory=fixture();auto source=model();
    memory.put(0x403c,std::uint32_t(0x30000));
    memory.put(0x402c,std::uint32_t(0));memory.put(0x4030,std::uint32_t(0));
    memory.put(0x4034,std::uint32_t(0));
    auto metadata=std::make_shared<AuthoredPoseData>();
    for(std::size_t i=0;i<count;++i){
        const auto address=std::uint32_t(0x30000+i*0x100);
        const auto name_pointer=std::uint32_t(0x50000+i*0x10);
        const auto parent=i?std::uint32_t(address-0x100):0u;
        const auto child=i+1<count?std::uint32_t(address+0x100):0u;
        const auto label="n"+std::to_string(i);
        memory.name(name_pointer,label.c_str());
        memory.node(address,0x7100,name_pointer,parent,child,0,0);
        AuthoredPoseNode node;node.name=label;
        node.parent=i?static_cast<std::uint32_t>(i-1):UINT32_MAX;
        node.local=identity;metadata->nodes.push_back(std::move(node));
    }
    source.pose=metadata;return {std::move(memory),std::move(source)};
}
}
int main(){
    auto source=model();auto memory=fixture();std::string error;
    std::shared_ptr<const ModelNodeTemplate> output;
    assert(capture_model_node_template(source,profile,&memory,Memory::read,output,error));
    assert(error.empty()&&memory.passes==2&&output->model==source.model&&
           output->authored==source.pose&&output->data->nodes.size()==6);
    assert(output->data->nodes[0].address==0x5100&&output->data->nodes[1].address==0x5000&&
           output->data->nodes[2].address==0x5200&&output->data->nodes[0].child_address==0x5300);
    assert(output->data->nodes[1].parent==UINT32_MAX&&
           output->data->nodes[1].first_child==2&&
           output->data->nodes[2].next_sibling==0&&
           output->data->nodes[0].parent==1&&
           output->data->nodes[0].first_child==3&&
           output->data->nodes[0].next_sibling==4&&
           output->data->nodes[5].next_sibling==UINT32_MAX);
    assert(output->data->hardpoints==0&&output->data->damage==2&&output->data->lights==3&&
           output->data->nodes[0].flags==3&&output->data->nodes[0].scale==1);
    auto retained=output;source={};
    assert(retained->model&&retained->authored&&retained->data->nodes[0].name=="mesh");
    source=model();
    for(auto change:{Change::header,Change::role,Change::db_name_pointer,
                     Change::db_name_byte,Change::name_pointer,Change::name_byte,
                     Change::type,Change::owner,Change::parent,Change::child,
                     Change::sibling,Change::matrix,Change::scale,Change::flags,
                     Change::unreadable}){
        memory=fixture();memory.change=change;reject(memory,source,output);
    }
    memory=fixture();memory.put(0x5118,std::uint32_t(0));reject(memory,source,output);
    memory=fixture();memory.put(0x5018,std::uint32_t(0x4000));reject(memory,source,output);
    memory=fixture();memory.put(0x5020,std::uint32_t(0x5100));reject(memory,source,output);
    memory=fixture();memory.put(0x5120,std::uint32_t(0x5100));reject(memory,source,output);
    memory=fixture();memory.put(0x501c,std::uint32_t(0xfffffffc));reject(memory,source,output);
    memory=fixture();memory.put(0x4030,std::uint32_t(0xdead));reject(memory,source,output);
    memory=fixture();memory.name(0x9100,"foreign.sod");reject(memory,source,output);
    memory=fixture();memory.put(0x4038,std::uint32_t(0xdead));reject(memory,source,output);
    memory=fixture();memory.put(0x5304,std::uint32_t(0x4001));reject(memory,source,output);
    memory=fixture();memory.put(0x7124,std::uint32_t(0x7130));reject(memory,source,output);
    memory=fixture();memory.name(0x9000,"wrong");reject(memory,source,output);
    memory=fixture();memory.name(0x9060,"extra");
    memory.node(0x5600,0x7100,0x9060,0x5000,0,0,7);
    memory.put(0x5520,std::uint32_t(0x5600));reject(memory,source,output);
    memory=fixture();memory.put(0x5154,NAN);reject(memory,source,output);
    auto bad=model();auto copy=std::make_shared<AuthoredPoseData>(*bad.pose);
    copy->nodes[0].name="root";bad.pose=copy;reject(fixture(),bad,output);
    bad=model();copy=std::make_shared<AuthoredPoseData>(*bad.pose);
    copy->nodes[0].parent=UINT32_MAX;bad.pose=copy;reject(fixture(),bad,output);
    bad=model();copy=std::make_shared<AuthoredPoseData>(*bad.pose);
    copy->nodes[0].parent=999;bad.pose=copy;reject(fixture(),bad,output);
    bad=model();auto altered=std::make_shared<ModelAsset>(*bad.model);
    altered->info.byte_count=2;bad.model=altered;reject(fixture(),bad,output);
    bad=model();altered=std::make_shared<ModelAsset>(*bad.model);
    altered->info.name[0]=0;bad.model=altered;reject(fixture(),bad,output);
    bad=model();altered=std::make_shared<ModelAsset>(*bad.model);
    altered->info.database=0;bad.model=altered;reject(fixture(),bad,output);
    bad=model();altered=std::make_shared<ModelAsset>(*bad.model);
    altered->info.serial=0;bad.model=altered;reject(fixture(),bad,output);
    auto [within_depth,within_source]=chain(256);
    if(!capture_model_node_template(within_source,profile,&within_depth,Memory::read,output,error)){
        std::fprintf(stderr,"depth 256 capture: %s\n",error.c_str());return 1;
    }
    assert(output->data->nodes.size()==256&&output->data->nodes.back().parent_address!=0);
    auto [too_deep,deep_source]=chain(257);
    reject(std::move(too_deep),deep_source,output);
}
