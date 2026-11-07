#include "../renderer/native11/craft_view_capture.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t bits(float value){std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
struct Memory {
    std::map<std::uint32_t,std::vector<std::uint8_t>> bytes;
    std::vector<std::uint32_t> addresses;
    unsigned flip_at=0;
    std::uint32_t flip_address=0;
    std::vector<std::uint8_t> flip_bytes;
    void word(std::uint32_t address,std::uint32_t value){
        auto& out=bytes[address];out.resize(4);std::memcpy(out.data(),&value,4);
    }
    void byte(std::uint32_t address,std::uint8_t value){bytes[address]={value};}
};
bool read(void* raw,std::uint32_t address,void* out,std::size_t size){
    auto& memory=*static_cast<Memory*>(raw);memory.addresses.push_back(address);
    if(memory.flip_at&&memory.addresses.size()==memory.flip_at)
        memory.bytes[memory.flip_address]=memory.flip_bytes;
    const auto found=memory.bytes.find(address);
    if(found==memory.bytes.end()||found->second.size()!=size)return false;
    std::memcpy(out,found->second.data(),size);return true;
}
struct Fixture {
    Object object{};CraftViewReadProfile profile{};Memory memory{};
    std::uint32_t camera=0x7000,control=encode_native_x87(0x037f);
    Fixture(){
        object.entity=0x1000;object.instance=0x2000;object.database=0x3000;
        object.instance_vtable=0x9000;object.database_vtable=0x9004;
        object.model_serial=1;object.model_identity=ModelIdentity::plain_database_name;
        object.placement_identity=PlacementIdentity::craft_runtime_scale;
        profile={0x4000,0x4004,0x4008,0x400c,0x4010,0x9000,0x9004,0x4014};
        auto& m=memory;
        m.word(0x1004,0x2000);m.word(0x2000,0x9000);m.word(0x2080,0x3000);
        m.word(0x3000,0x9004);m.word(0x4000,0x5000);m.word(0x50fc,camera);
        m.word(0x50e8,0);m.word(0x4004,0x6000);m.word(0x6080,0x6100);
        m.word(0x6140,1);m.byte(0x6084,0);m.word(0x4008,1);
        m.byte(0x400c,0);m.word(0x4010,bits(10.f));
        m.word(0x4014,bits(.5f));m.word(0x4018,bits(2.f));m.word(0x401c,bits(.875f));
        for(unsigned i=0;i<3;++i){m.word(0x70e4+4*i,bits(0.f));
            m.word(0x10ac+4*i,bits(i==0?3.f:i==1?4.f:0.f));}
        m.word(0x2040,bits(1.f));m.byte(0x2084,0);
        // These stale cached values are never authorized inputs.
        m.word(0x5088,bits(999.f));m.word(0x508c,bits(999.f));
    }
    bool capture(CapturedCraftView& out,std::string& error){
        memory.addresses.clear();return capture_craft_view(object,camera,control,profile,&memory,read,out,error);
    }
};
std::vector<std::uint8_t> word_bytes(std::uint32_t value){
    std::vector<std::uint8_t> out(4);std::memcpy(out.data(),&value,4);return out;
}
void rejected(Fixture& fixture,const char* why){
    CapturedCraftView output{};output.engine=0xdeadbeef;output.result.distance=123.f;
    std::string error="stale";
    need(!fixture.capture(output,error)&&!error.empty()&&
         output.engine==0xdeadbeef&&output.result.distance==123.f,why);
}
void cases(){
    Fixture stable;CapturedCraftView out{};std::string error="stale";
    need(stable.capture(out,error)&&error.empty()&&stable.memory.addresses.size()==50&&
         out.engine==0x5000&&out.game==0x6000&&out.settings==0x6100&&
         out.camera==stable.camera&&out.entity==stable.object.entity&&
         out.instance==stable.object.instance&&out.database==stable.object.database&&
         out.native_x87_control==stable.control&&out.input.position[0]==3.f&&
         out.input.position[1]==4.f&&out.input.radius==1.f&&out.input.max_distance==10.f&&
         out.input.near_fade_min==.5f&&out.input.near_fade_max==2.f&&
         out.input.near_fade_amount==.875f&&
         std::fabs(out.result.weighted_distance-std::sqrt(10.f))<.0001f&&
         std::fabs(out.result.distance-5.f)<.0001f,
         "fresh two-pass fields feed Craft policy and retain IDs");
    for(auto address:stable.memory.addresses)
        need(address!=0x5088&&address!=0x508c,"stale engine 88/8c values never read");
    auto first_distance=out.result.distance;
    stable.memory.word(0x70e4,bits(1.f));
    need(stable.capture(out,error)&&out.input.camera[0]==1.f&&
         out.result.distance!=first_distance,"fresh camera coordinate changes result");
    Fixture near;near.memory.word(0x50e8,1);near.memory.word(0x10ac,bits(3.f));
    near.memory.word(0x10b0,bits(0.f));near.memory.word(0x2040,bits(3.f));
    need(near.capture(out,error)&&out.result.fade_written,
         "near fade branch evaluates captured native parameters");
    const auto original_alpha=out.result.alpha;
    near.memory.word(0x4014,bits(.9f));near.memory.word(0x4018,bits(.99f));
    near.memory.word(0x401c,bits(.9f));
    need(near.capture(out,error)&&out.input.near_fade_min==.9f&&
         out.input.near_fade_max==.99f&&out.input.near_fade_amount==.9f&&
         out.result.alpha!=original_alpha,
         "changed runtime near-fade parameters change evaluated alpha");
    Fixture bad;bad.object.model_serial=0;rejected(bad,"unversioned model rejected");
    need(bad.memory.addresses.empty(),"unversioned model rejected before read");
    bad=Fixture{};bad.object.model_identity=ModelIdentity::unresolved;
    rejected(bad,"unresolved model identity rejected");
    bad=Fixture{};bad.object.placement_identity=PlacementIdentity::authored_placement;
    rejected(bad,"noncraft placement rejected");
    bad=Fixture{};bad.profile.instance_vtable=0x9001;
    rejected(bad,"object and profile vtable mismatch rejected before read");
    bad=Fixture{};bad.profile.engine_pointer=std::numeric_limits<std::uint32_t>::max()-1;
    rejected(bad,"full-span pointer overflow rejected");
    bad=Fixture{};bad.profile.near_fade_pointer=0;
    rejected(bad,"missing near-fade pointer rejected");
    need(bad.memory.addresses.empty(),"missing near-fade profile rejected before read");
    bad=Fixture{};bad.memory.word(0x1004,0x2001);rejected(bad,"entity-instance mismatch rejected");
    bad=Fixture{};bad.memory.word(0x50fc,0x7001);rejected(bad,"engine camera mismatch rejected");
    bad=Fixture{};bad.memory.word(0x2080,0x3001);rejected(bad,"database owner mismatch rejected");
    bad=Fixture{};bad.memory.word(0x4010,bits(0.f));rejected(bad,"zero max distance rejected");
    bad=Fixture{};bad.memory.word(0x4014,bits(2.f));bad.memory.word(0x4018,bits(1.f));
    rejected(bad,"reversed near-fade interval rejected");
    bad=Fixture{};bad.memory.word(0x4014,bits(std::numeric_limits<float>::quiet_NaN()));
    rejected(bad,"nonfinite near-fade minimum rejected");
    bad=Fixture{};bad.memory.word(0x401c,bits(std::numeric_limits<float>::quiet_NaN()));
    rejected(bad,"nonfinite near-fade amount rejected");
    bad=Fixture{};bad.memory.word(0x70e4,bits(std::numeric_limits<float>::quiet_NaN()));
    rejected(bad,"nonfinite camera rejected atomically");
    bad=Fixture{};bad.memory.word(0x10ac,bits(1000001.f));
    rejected(bad,"oversize logical position rejected atomically");
    bad=Fixture{};bad.memory.flip_at=26;bad.memory.flip_address=0x70e4;
    bad.memory.flip_bytes=word_bytes(bits(1.f));
    rejected(bad,"camera changed between snapshots rejected");
    bad=Fixture{};bad.memory.flip_at=26;bad.memory.flip_address=0x4004;
    bad.memory.flip_bytes=word_bytes(0x6001);
    rejected(bad,"game pointer changed between snapshots rejected");
    bad=Fixture{};bad.memory.flip_at=26;bad.memory.flip_address=0x2080;
    bad.memory.flip_bytes=word_bytes(0x3001);
    rejected(bad,"model database changed between snapshots rejected");
    bad=Fixture{};bad.memory.word(0x50e8,1);bad.memory.flip_at=26;
    bad.memory.flip_address=0x50e8;bad.memory.flip_bytes=word_bytes(2);
    rejected(bad,"raw nonzero fade flag change between snapshots rejected");
    bad=Fixture{};bad.memory.flip_at=26;bad.memory.flip_address=0x4014;
    bad.memory.flip_bytes=word_bytes(bits(.9f));
    rejected(bad,"near-fade parameter change between snapshots rejected");
}
}
int main(){cases();std::puts("PASS native11 craft view capture");}
