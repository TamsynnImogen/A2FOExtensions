#include "../renderer/native11/model_lod_template.hpp"
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
constexpr std::uint32_t lod_type=0x7110;
enum class Change {none,centre_bits,radius,pointer,payload_pointer,count,link,payload,owner,type};
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    Change change=Change::none;
    unsigned passes=0,forbidden=0;
    std::uint32_t unreadable=0;
    template<class T> void put(std::uint32_t address,T value){
        const auto* p=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t i=0;i<sizeof(T);++i)bytes[address+i]=p[i];
    }
    void list(std::uint32_t node,std::uint32_t next,std::uint32_t prev,std::uint32_t payload){
        put(node,next);put(node+4,prev);put(node+8,payload);
    }
    void control(std::uint32_t address,std::uint32_t sentinel,std::uint32_t count,
                 std::array<float,3> centre,float radius){
        put(address,std::uint32_t(0x7000));put(address+4,std::uint32_t(0x4000));
        put(address+0xc8,sentinel);put(address+0xcc,count);
        for(std::size_t i=0;i<3;++i)put(address+0xd0+4*i,centre[i]);
        put(address+0xdc,radius);
    }
    void mutate(){
        switch(change){
        case Change::centre_bits:put(0x50d4,-0.0f);break;
        case Change::radius:put(0x50dc,6.0f);break;
        case Change::pointer:put(0x50c8,std::uint32_t(0x8500));break;
        case Change::payload_pointer:put(0x8108,std::uint32_t(0x9030));break;
        case Change::count:put(0x50cc,std::uint32_t(1));break;
        case Change::link:put(0x8100,std::uint32_t(0x8000));break;
        case Change::payload:put(0x9004,std::uint32_t(201));break;
        case Change::owner:put(0x5004,std::uint32_t(0x4001));break;
        case Change::type:put(0x7004,std::uint32_t(0x7001));break;
        case Change::none:break;
        }
    }
    static bool read(void* raw,std::uint32_t address,void* output,std::size_t length){
        auto& m=*static_cast<Memory*>(raw);
        if(address==0x5000&&++m.passes==2)m.mutate();
        for(auto base:{0x5000u,0x5100u,0x5200u})
            if(address==base+0xc0||address==base+0xbc||address==base+0x24){
                ++m.forbidden;return false;
            }
        if(address==m.unreadable||length>UINT32_MAX-address)return false;
        auto* dest=static_cast<std::uint8_t*>(output);
        for(std::size_t i=0;i<length;++i){
            const auto found=m.bytes.find(address+static_cast<std::uint32_t>(i));
            if(found==m.bytes.end())return false;
            dest[i]=found->second;
        }
        return true;
    }
};
Memory fixture(){
    Memory m;m.put(0x7004,lod_type);
    m.control(0x5000,0x8000,2,{1,0,3},5);
    m.list(0x8000,0x8100,0x8200,0);
    m.list(0x8100,0x8200,0x8000,0x9000);
    m.list(0x8200,0x8000,0x8100,0x9010);
    m.put(0x9000,std::uint32_t(0x6000));m.put(0x9004,std::uint32_t(200));
    m.put(0x9030,std::uint32_t(0x6000));m.put(0x9034,std::uint32_t(200));
    m.put(0x9010,std::uint32_t(0x6100));m.put(0x9014,std::uint32_t(50));
    m.list(0x8500,0x8100,0x8200,0); // same contents, different sentinel identity
    m.control(0x5100,0x8300,1,{0,2,0},3);
    m.list(0x8300,0x8310,0x8310,0);
    m.list(0x8310,0x8300,0x8300,0x9020);
    m.put(0x9020,std::uint32_t(0x6200));m.put(0x9024,std::uint32_t(5));
    m.control(0x5200,0x8400,0,{0,0,0},0);
    m.list(0x8400,0x8400,0x8400,0);
    return m;
}
ModelNodeTemplate baseline(){
    auto model=std::make_shared<ModelAsset>();model->info.database=0x4000;model->info.serial=7;
    auto authored=std::make_shared<AuthoredPoseData>();
    auto data=std::make_shared<ModelNodeTemplateData>();
    for(auto [address,parent,type]:{
        std::tuple{0x5000u,UINT32_MAX,lod_type},
        std::tuple{0x6000u,0u,0x7200u},
        std::tuple{0x6100u,0u,0x7200u},
        std::tuple{0x5100u,1u,lod_type},
        std::tuple{0x6200u,3u,0x7200u},
        std::tuple{0x5200u,0u,lod_type}}){
        ModelNodeTemplateNode node;node.address=address;node.parent=parent;
        node.type_function=type;node.vtable=type==lod_type?0x7000:0x7200;
        data->nodes.push_back(node);authored->nodes.emplace_back();
    }
    return {model,authored,data};
}
void reject(Memory memory,const ModelNodeTemplate& model,
            std::shared_ptr<const ModelLodTemplateData>& output){
    const auto old=output;std::string error;
    assert(!capture_model_lod_template(model,lod_type,&memory,Memory::read,output,error));
    assert(output==old&&!error.empty());
}
}
int main(){
    auto model=baseline();auto memory=fixture();std::string error;
    std::shared_ptr<const ModelLodTemplateData> output;
    assert(capture_model_lod_template(model,lod_type,&memory,Memory::read,output,error));
    assert(error.empty()&&memory.passes==2&&memory.forbidden==0&&output->controls.size()==3);
    assert(output->controls[0].node_index==0&&output->controls[0].centre[0]==1&&
           output->controls[0].radius==5&&output->controls[0].levels.size()==2&&
           output->controls[0].levels[0].node_index==1&&
           output->controls[0].levels[0].face_count==200&&
           output->controls[0].levels[1].node_index==2&&
           output->controls[0].levels[1].face_count==50);
    assert(output->controls[1].node_index==3&&output->controls[1].levels[0].node_index==4&&
           output->controls[2].node_index==5&&output->controls[2].levels.empty());
    auto retained=output;std::weak_ptr<const ModelAsset> weak=model.model;model={};
    assert(weak.expired()&&retained->controls[0].levels[0].face_count==200);
    model=baseline();
    for(auto change:{Change::centre_bits,Change::radius,Change::pointer,
                     Change::payload_pointer,Change::count,
                     Change::link,Change::payload,Change::owner,Change::type}){
        memory=fixture();memory.change=change;reject(memory,model,output);
    }
    memory=fixture();memory.put(0x5004,std::uint32_t(0x4001));reject(memory,model,output);
    memory=fixture();memory.put(0x7004,std::uint32_t(0x7001));reject(memory,model,output);
    memory=fixture();memory.put(0x50dc,-1.0f);reject(memory,model,output);
    memory=fixture();memory.put(0x50dc,NAN);reject(memory,model,output);
    memory=fixture();memory.put(0x50d0,NAN);reject(memory,model,output);
    memory=fixture();memory.put(0x8104,std::uint32_t(0xdead));reject(memory,model,output);
    memory=fixture();memory.put(0x8200,std::uint32_t(0x8100));reject(memory,model,output);
    memory=fixture();memory.put(0x8004,std::uint32_t(0x8100));reject(memory,model,output);
    memory=fixture();memory.put(0x50cc,std::uint32_t(4097));reject(memory,model,output);
    memory=fixture();memory.put(0x8208,std::uint32_t(0x9000));reject(memory,model,output);
    memory=fixture();memory.put(0x9010,std::uint32_t(0x6000));reject(memory,model,output);
    memory=fixture();memory.put(0x9010,std::uint32_t(0x6200));reject(memory,model,output);
    memory=fixture();memory.put(0x9010,std::uint32_t(0xdead));reject(memory,model,output);
    memory=fixture();memory.put(0x50c8,std::uint32_t(0xfffffffc));reject(memory,model,output);
    memory=fixture();memory.unreadable=0x9004;reject(memory,model,output);
    auto bad=baseline();auto data=std::make_shared<ModelNodeTemplateData>(*bad.data);
    data->nodes[1].parent=5;bad.data=data;reject(fixture(),bad,output);
}
