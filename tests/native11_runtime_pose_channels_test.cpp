#include "../renderer/native11/runtime_pose_channels.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
constexpr std::array<float,12> id{1,0,0,0,1,0,0,0,1,0,0,0};
constexpr RuntimePoseReadProfile profile{{0x1000,0x7000,0x7100},0x7200,0x7300,0x7400,0x7500};
enum class Change {none,list,owner,name,target,config,flag,times,keys,clock};
struct Memory {
    std::array<std::uint8_t,0x10000> bytes{},present{};
    Change change=Change::none;
    unsigned list_pass=0,clock_pass=0,forbidden=0;
    template<class T> void put(std::uint32_t address,T value) {
        assert(address+sizeof(T)<=bytes.size());
        std::memcpy(bytes.data()+address,&value,sizeof(T));
        for(std::size_t i=0;i<sizeof(T);++i)present[address+i]=1;
    }
    void put_name(std::uint32_t address,const char* name) {
        for(std::size_t i=0;i<=std::strlen(name);++i)put(address+i,std::uint8_t(name[i]));
    }
    void put_floats(std::uint32_t address,const std::vector<float>& values) {
        for(std::size_t i=0;i<values.size();++i)put(address+i*4,values[i]);
    }
    static bool read(void* raw,std::uint32_t address,void* output,std::size_t length) {
        auto& m=*static_cast<Memory*>(raw);
        if(address==0x6030||address==0x6130||address==0x2100){++m.forbidden;return false;}
        if(address==0x1000&&++m.clock_pass==3&&m.change==Change::clock)
            m.put(0x208c,6.0f);
        if(address==0x406c&&++m.list_pass==2){
            switch(m.change){
            case Change::list:m.put(0x5100,std::uint32_t(0x5000));break;
            case Change::owner:m.put(0x6004,std::uint32_t(0x4001));break;
            case Change::name:m.put(0x9000,std::uint8_t('X'));break;
            case Change::target:m.put(0x8004,std::uint32_t(0x4001));break;
            case Change::config:m.put(0x6038,0.5f);break;
            case Change::flag:m.put(0x6048,std::uint32_t(1));break;
            case Change::times:m.put(0xb004,1.5f);break;
            case Change::keys:m.put(0xa004,3.0f);break;
            default:break;
            }
        }
        if(address>=m.bytes.size()||length>m.bytes.size()-address)return false;
        for(std::size_t i=0;i<length;++i)if(!m.present[address+i])return false;
        std::memcpy(output,m.bytes.data()+address,length);return true;
    }
};
Object object(){
    Object o;o.instance=0x3000;o.instance_vtable=0x7000;
    o.database=0x4000;o.database_vtable=0x7100;o.model_serial=42;
    return o;
}
ModelPoseSnapshot authored(){
    auto model=std::make_shared<ModelAsset>();model->info.database=0x4000;model->info.serial=42;
    auto pose=std::make_shared<AuthoredPoseData>();
    AuthoredPoseNode child;child.name="child";child.parent=1;child.kind=1;child.local=id;
    AuthoredPoseNode root;root.name="root";root.local=id;
    pose->nodes={child,root};
    AuthoredPoseChannel matrix;matrix.node_index=0;matrix.channel_type=1;
    matrix.period=9;matrix.key_width=12;matrix.keys.insert(matrix.keys.end(),id.begin(),id.end());
    matrix.keys.insert(matrix.keys.end(),id.begin(),id.end());
    AuthoredPoseChannel scale;scale.node_index=1;scale.channel_type=5;
    scale.period=9;scale.key_width=1;scale.keys={9};
    pose->channels={matrix,scale};return {model,pose};
}
Memory fixture(){
    Memory m;
    m.put(0x1000,std::uint32_t(0x2000));m.put(0x208c,5.0f);
    m.put(0x3000,std::uint32_t(0x7000));m.put(0x300c,std::uint32_t(1));
    m.put(0x3018,4.0f);m.put(0x3076,std::uint8_t(1));
    m.put(0x3080,std::uint32_t(0x4000));m.put(0x4000,std::uint32_t(0x7100));
    m.put(0x406c,std::uint32_t(0x5000));m.put(0x4070,std::uint32_t(2));
    m.put(0x5000,std::uint32_t(0x5100));m.put(0x5004,std::uint32_t(0x5200));
    m.put(0x5100,std::uint32_t(0x5200));m.put(0x5104,std::uint32_t(0x5000));m.put(0x5108,std::uint32_t(0x6000));
    m.put(0x5200,std::uint32_t(0x5000));m.put(0x5204,std::uint32_t(0x5100));m.put(0x5208,std::uint32_t(0x6100));
    m.put(0x7210,std::uint32_t(0x7400));m.put(0x7310,std::uint32_t(0x7500));
    m.put_name(0x9000,"child");m.put_name(0x9010,"root");
    m.put_name(0x9020,"CHILD");m.put_name(0x9030,"ROOT");
    m.put(0x8004,std::uint32_t(0x4000));m.put(0x8008,std::uint32_t(0x9020));
    m.put(0x8104,std::uint32_t(0x4000));m.put(0x8108,std::uint32_t(0x9030));
    // Matrix runtime has one key although authored has two.
    m.put(0x6000,std::uint32_t(0x7300));m.put(0x6004,std::uint32_t(0x4000));
    m.put(0x6008,std::uint32_t(0x9000));m.put(0x600c,std::uint32_t(0x5100));
    m.put(0x6018,std::uint8_t(0));
    m.put(0x6020,std::uint32_t(0));m.put(0x6024,std::uint32_t(0));m.put(0x6028,std::uint32_t(0));
    m.put(0x602c,4.0f);m.put(0x6034,std::uint32_t(1));m.put(0x6038,0.0f);
    m.put(0x603c,std::uint32_t(0x8000));m.put(0x6040,std::uint32_t(0x24));
    m.put(0x6044,std::uint32_t(2));m.put(0x6048,std::uint32_t(0));
    m.put(0x604c,std::uint32_t(0xa100));m.put(0x6050,std::uint32_t(0xa130));m.put(0x6054,std::uint32_t(0xa130));
    auto matrix=id;matrix[9]=7.0f;
    m.put_floats(0xa100,{matrix.begin(),matrix.end()});
    // Scalar runtime has two keys plus an interpolation guard; authored has one.
    m.put(0x6100,std::uint32_t(0x7200));m.put(0x6104,std::uint32_t(0x4000));
    m.put(0x6108,std::uint32_t(0x9010));m.put(0x610c,std::uint32_t(0x5200));
    m.put(0x6118,std::uint8_t(5));
    m.put(0x6120,std::uint32_t(0xb000));m.put(0x6124,std::uint32_t(0xb008));m.put(0x6128,std::uint32_t(0xb008));
    m.put(0x612c,4.0f);m.put(0x6134,std::uint32_t(2));m.put(0x6138,0.0f);
    m.put(0x613c,std::uint32_t(0x8100));m.put(0x6140,std::uint32_t(0x54));
    m.put(0x6144,std::uint32_t(3));m.put(0x6148,std::uint32_t(0));
    m.put(0x614c,std::uint32_t(0xa000));m.put(0x6150,std::uint32_t(0xa00c));m.put(0x6154,std::uint32_t(0xa00c));
    m.put_floats(0xa000,{1,2,3});m.put_floats(0xb000,{0,2});
    return m;
}
void reject(Memory m,const ModelPoseSnapshot& source,const char* label){
    RuntimePoseChannels out;out.clock.instance=123;out.model=source;
    std::string error;
    assert(!capture_runtime_pose_channels(object(),source,profile,&m,Memory::read,out,error));
    assert(out.clock.instance==123&&out.model.model==source.model&&out.model.pose==source.pose&&
           !error.empty());
    (void)label;
}
}
int main(){
    auto source=authored();auto m=fixture();RuntimePoseChannels out;std::string error;
    assert(capture_runtime_pose_channels(object(),source,profile,&m,Memory::read,out,error));
    assert(error.empty()&&m.list_pass==2&&m.clock_pass==4&&m.forbidden==0);
    assert(out.model.model==source.model&&out.model.pose!=source.pose&&out.clock.elapsed==1);
    assert(out.model.pose->channels[0].keys.size()==12&&out.model.pose->channels[0].keys[9]==7);
    assert(out.model.pose->channels[1].keys==std::vector<float>({1,2})&&
           out.playback[1].has_guard&&out.playback[1].scalar_guard==3&&
           out.playback[1].timestamps==std::vector<float>({0,2}));
    OwnedAnimationPose evaluated;
    assert(evaluate_owned_animation_pose(out.model,out.clock,out.playback,evaluated,error));
    assert(evaluated.local[0][9]==7&&evaluated.scales[1]>=1&&evaluated.scales[1]<=3);
    m=fixture();m.put(0x6044,std::uint32_t(3));
    m.put(0x6050,std::uint32_t(0xa160));m.put(0x6054,std::uint32_t(0xa160));
    auto guard=id;guard[9]=999;
    m.put_floats(0xa130,{guard.begin(),guard.end()});
    assert(capture_runtime_pose_channels(object(),source,profile,&m,Memory::read,out,error)&&
           out.model.pose->channels[0].keys.size()==12&&!out.playback[0].has_guard);
    assert(evaluate_owned_animation_pose(out.model,out.clock,out.playback,evaluated,error)&&
           evaluated.local[0][9]==7); // native matrix mode 3 never plays its guard
    for(Change change:{Change::list,Change::owner,Change::name,Change::target,
                       Change::config,Change::flag,Change::times,Change::keys,Change::clock}){
        m=fixture();m.change=change;reject(m,source,"between-pass mutation");
    }
    m=fixture();m.put(0x6040,std::uint32_t(0x54));reject(m,source,"wrong target offset");
    m=fixture();m.put_name(0x9020,"other");reject(m,source,"target name mismatch");
    m=fixture();m.put(0x8004,std::uint32_t(0x4100));reject(m,source,"target owner mismatch");
    m=fixture();m.put(0x7310,std::uint32_t(0x7401));reject(m,source,"wrong play entry");
    m=fixture();m.put(0x6108,std::uint32_t(0x9000));reject(m,source,"duplicate name target");
    m=fixture();m.put(0x6070,std::uint32_t(257)); // unrelated bytes cannot affect capture
    RuntimePoseChannels repeat;assert(capture_runtime_pose_channels(object(),source,profile,&m,Memory::read,repeat,error));
    m=fixture();m.put(0x4070,std::uint32_t(257));reject(m,source,"channel limit");
    m=fixture();m.put(0x6034,std::uint32_t(0));reject(m,source,"zero keys");
    m=fixture();m.put(0x6134,std::uint32_t(65536));reject(m,source,"key count limit");
    m=fixture();m.put(0xb004,0.0f);reject(m,source,"nonincreasing timestamps");
    m=fixture();m.put(0xa004,NAN);reject(m,source,"nonfinite key");
    auto references=authored();auto copy=std::make_shared<AuthoredPoseData>(*references.pose);
    copy->reference_count=1;references.pose=copy;reject(fixture(),references,"unsupported references");
    std::puts("PASS native11 runtime pose channels");
}
