#include "../renderer/native11/scene_capture.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>

using namespace a2fo::native11;
namespace {
void need(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
void close(float actual, float expected, const char* message) {
    need(std::fabs(actual-expected) < 0.00001f, message);
}
struct Memory {
    std::map<std::uint32_t, unsigned char> bytes;
    std::uint32_t fail_at = 0;
    std::uint32_t forbidden_at = 0;
    bool read_forbidden = false;
    int reads = 0;
    template<class T> void put(std::uint32_t address, const T& value) {
        const auto* source = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t i=0; i<sizeof(value); ++i) bytes[address+static_cast<std::uint32_t>(i)]=source[i];
    }
    static bool read(void* context, std::uint32_t address, void* output, std::size_t length) {
        auto& memory=*static_cast<Memory*>(context);
        ++memory.reads;
        if (address == memory.forbidden_at) memory.read_forbidden = true;
        if (address == memory.fail_at) return false;
        auto* target=static_cast<unsigned char*>(output);
        for (std::size_t i=0; i<length; ++i) {
            const auto found=memory.bytes.find(address+static_cast<std::uint32_t>(i));
            if (found==memory.bytes.end()) return false;
            target[i]=found->second;
        }
        return true;
    }
};
constexpr std::uint32_t camera=0x1000, entity=0x2000, instance=0x3000;
constexpr std::array<float,16> screen={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
constexpr std::array<float,4> viewport={3,7,640,480};
constexpr std::array<float,12> matrix={0,1,0, -1,0,0, 0,0,1, 10,20,30};
void fixture(Memory& m) {
    m.put(entity+4,instance);
    m.put(instance+0x44,matrix);
    m.put(camera+0x160,screen);
    m.put(camera+0x1b0,viewport);
}
void preserved(const Frame& actual, const Frame& before, const char* message) {
    need(std::memcmp(&actual,&before,sizeof(Frame))==0,message);
}
}
int main() {
    Memory memory; fixture(memory);
    Collector capture(&memory,Memory::read);
    Frame result{};
    need(!capture.begin(0,true),"null camera rejected");
    need(!capture.begin(camera,false),"non-main camera rejected");
    need(capture.begin(camera,true),"main begin");
    EngineCameraView live_view{};
    need(capture.camera_view(camera,live_view),"active camera sampled before frame end");
    need(std::memcmp(live_view.viewport,viewport.data(),sizeof(viewport))==0,"live viewport copied");
    const auto saved_live=live_view;
    need(!capture.camera_view(camera+1,live_view)&&
         !std::memcmp(&live_view,&saved_live,sizeof(live_view)),"foreign live camera preserves output");
    memory.fail_at=camera+0x160;
    need(!capture.camera_view(camera,live_view)&&
         !std::memcmp(&live_view,&saved_live,sizeof(live_view)),"unreadable live camera preserves output");
    memory.fail_at=0;
    need(!capture.begin(camera,true),"nested begin rejected");
    need(!capture.end(camera+1,result),"foreign end rejected");
    capture.observe_post_frustum(entity,camera);
    // Objects suppressed by the engine's prior frustum/visibility gates are never supplied.
    need(capture.end(camera,result),"matching end publishes");
    need(!capture.camera_view(camera,live_view),"ended capture cannot sample live camera");
    need(result.camera==camera && result.generation==1 && result.object_count==1,
         "owned visible count and generation");
    need(result.objects[0].entity==entity && result.objects[0].instance==instance,
         "telemetry identifiers copied");
    const float expected[16]={0,-1,0,10, 1,0,0,20, 0,0,1,30, 0,0,0,1};
    for (int i=0;i<16;++i) close(result.objects[0].world[i],expected[i],"rotated translated matrix");
    need(std::memcmp(result.engine_screen,screen.data(),sizeof(screen))==0,"raw camera copied");
    need(std::memcmp(result.view.viewport,viewport.data(),sizeof(viewport))==0,"viewport copied");
    close(result.view.world_to_clip[0],2*(639.f/640.f),"canonical clip conversion");
    const Frame published=result;
    // Removing source memory after publication cannot change the owned result.
    memory.bytes.clear();
    preserved(result,published,"published frame owns data");
    fixture(memory);

    need(capture.begin(camera,true),"begin empty frame");
    need(capture.end(camera,result),"empty view publishes");
    need(result.object_count==0 && result.generation==2 &&
         result.objects[0].entity==0 && result.objects[0].instance==0,
         "repeated frame clears objects");

    auto failure=[&](const char* label) {
        const Frame before=result;
        need(!capture.end(camera,result),label);
        preserved(result,before,"failure preserves published output");
        need(capture.begin(camera,true),"failure deactivates capture");
        capture.abort();
    };
    need(capture.begin(camera,true),"begin missing pointer");
    memory.fail_at=entity+4;
    capture.observe_post_frustum(entity,camera); failure("unreadable pointer drops frame");
    memory.fail_at=0;
    need(capture.begin(camera,true),"begin overflowing entity");
    capture.observe_post_frustum(0xfffffffeu,camera);
    failure("entity pointer offset overflow drops frame");
    need(capture.begin(camera,true),"begin null instance");
    memory.put(entity+4,std::uint32_t{0});
    capture.observe_post_frustum(entity,camera); failure("null instance drops frame");
    fixture(memory);
    need(capture.begin(camera,true),"begin overflowing instance");
    memory.put(entity+4,std::uint32_t{0xfffffff0});
    const int reads_before=memory.reads;
    capture.observe_post_frustum(entity,camera);
    need(memory.reads==reads_before+1,"overflow rejected before matrix read");
    failure("overflow drops frame"); fixture(memory);
    need(capture.begin(camera,true),"begin unreadable matrix");
    memory.fail_at=instance+0x44;
    capture.observe_post_frustum(entity,camera); failure("unreadable matrix drops frame");
    memory.fail_at=0;
    auto bad=matrix; bad[2]=std::numeric_limits<float>::quiet_NaN();
    memory.put(instance+0x44,bad);
    need(capture.begin(camera,true),"begin NaN matrix");
    capture.observe_post_frustum(entity,camera); failure("NaN drops frame");
    bad=matrix; bad[0]=bad[1]=bad[2]=0;
    memory.put(instance+0x44,bad);
    need(capture.begin(camera,true),"begin singular matrix");
    capture.observe_post_frustum(entity,camera); failure("singular drops frame"); fixture(memory);
    need(capture.begin(camera,true),"begin foreign observe");
    capture.observe_post_frustum(entity,camera+1);
    capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result) && result.object_count==1,"foreign observation ignored without contaminating main view");
    need(capture.begin(camera,true),"begin bad camera");
    memory.fail_at=camera+0x160; failure("unreadable camera drops frame");
    memory.fail_at=0;
    need(capture.begin(camera,true),"begin bad viewport");
    memory.fail_at=camera+0x1b0; failure("unreadable viewport drops frame");
    memory.fail_at=0;
    auto bad_screen=screen; bad_screen[0]=std::numeric_limits<float>::quiet_NaN();
    memory.put(camera+0x160,bad_screen);
    need(capture.begin(camera,true),"begin nonfinite camera");
    failure("nonfinite camera conversion drops frame"); fixture(memory);
    bad_screen=screen; bad_screen[0]=0;
    memory.put(camera+0x160,bad_screen);
    need(capture.begin(camera,true),"begin singular camera");
    failure("singular camera conversion drops frame"); fixture(memory);
    auto bad_viewport=viewport; bad_viewport[2]=0;
    memory.put(camera+0x1b0,bad_viewport);
    need(capture.begin(camera,true),"begin invalid viewport");
    failure("invalid viewport drops frame"); fixture(memory);
    need(capture.begin(camera,true),"begin capacity frame");
    for (std::size_t i=0;i<capacity;++i) capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result),"2048 observations publish");
    need(result.object_count==capacity,"capacity count");
    need(capture.begin(camera,true),"begin overflow frame");
    for (std::size_t i=0;i<capacity+1;++i) capture.observe_post_frustum(entity,camera);
    failure("2049th observation drops entire frame");
    need(capture.begin(camera,true),"begin abort frame");
    capture.observe_post_frustum(entity,camera); capture.abort();
    need(!capture.end(camera,result),"abort prevents publication");
    need(capture.begin(camera,true),"begin after abort");
    need(capture.end(camera,result) && result.object_count==0,"abort clears pending frame");

    // The verified plain ST3D_Database path supplies the original filename;
    // the synthetic ODF label is deliberately unrelated and never read.
    constexpr std::uint32_t entity2=0x2100,instance2=0x3100,db=0x4000;
    constexpr std::uint32_t name=0x5000,plain_vtable=0x6bc56c;
    const char* odf_label="other_ship.odf";
    (void)odf_label;
    memory.put(entity2+4,instance2);
    memory.put(instance2+0x44,matrix);
    memory.put(instance+0x80,db);
    memory.put(instance2+0x80,db);
    memory.put(db,plain_vtable);
    memory.put(db+0x38,name);
    static constexpr char filename[]="models/actual_ship.sod";
    memory.put(name,filename);
    need(capture.configure_plain_database_vtable(plain_vtable),"configure plain database at startup");
    std::uint64_t serial=41;
    const auto version=+[](void* context,std::uint32_t actual_db,const char* actual_name)->std::uint64_t {
        need(actual_db==db&&!std::strcmp(actual_name,filename),"serial reader receives copied plain identity");
        return *static_cast<std::uint64_t*>(context);
    };
    need(capture.configure_model_serial_reader(&serial,version),"configure version reader between frames");
    need(capture.begin(camera,true),"begin named frame");
    need(!capture.configure_model_serial_reader(nullptr,nullptr),"cannot reconfigure version reader during frame");
    need(!capture.configure_plain_database_vtable(plain_vtable+4),"cannot reconfigure active frame");
    capture.observe_post_frustum(entity,camera);
    serial=42;
    capture.observe_post_frustum(entity2,camera);
    need(capture.end(camera,result),"two named instances publish");
    need(result.object_count==2,"two instances captured");
    need(result.objects[0].model_serial==41&&result.objects[1].model_serial==42,"version copied at each observation");
    need(capture.configure_model_serial_reader(nullptr,nullptr),"disable version callback after frame");
    for (std::size_t i=0;i<2;++i) {
        const auto& object=result.objects[i];
        need(object.database==db && object.database_vtable==plain_vtable,
             "shared database identifiers copied");
        need(object.model_identity==ModelIdentity::plain_database_name &&
             std::strcmp(object.model_name,filename)==0 &&
             std::strcmp(object.model_name,odf_label)!=0,
             "original model name resolved independently of ODF label");
    }
    const Frame named=result;
    memory.bytes.clear();
    preserved(result,named,"model filename owns bytes after source removal");
    fixture(memory);
    memory.put(instance+0x80,db);
    memory.put(db,plain_vtable);
    memory.put(db+0x38,name);
    memory.put(name,filename);

    auto check_identity=[&](ModelIdentity expected_identity,const char* label) {
        need(capture.begin(camera,true),"begin identity case");
        capture.observe_post_frustum(entity,camera);
        need(capture.end(camera,result),label);
        need(result.object_count==1 && result.objects[0].model_identity==expected_identity,
             "identity state preserves valid placement");
        if (expected_identity!=ModelIdentity::plain_database_name)
            need(result.objects[0].model_name[0]==0,"failed name is zeroed");
    };
    memory.put(db,std::uint32_t{plain_vtable+4});
    memory.forbidden_at=db+0x38;
    check_identity(ModelIdentity::unsupported_database,"unsupported database publishes placement");
    need(!memory.read_forbidden,"wrong vtable never reads name pointer");
    memory.forbidden_at=0;
    memory.put(db,plain_vtable);
    memory.put(instance+0x80,std::uint32_t{0});
    check_identity(ModelIdentity::unresolved,"null database unresolved");
    memory.put(instance+0x80,db);
    memory.fail_at=instance+0x80;
    check_identity(ModelIdentity::unresolved,"unreadable database pointer unresolved");
    memory.fail_at=db;
    check_identity(ModelIdentity::unresolved,"unreadable database vtable unresolved");
    memory.fail_at=db+0x38;
    check_identity(ModelIdentity::unresolved,"unreadable name pointer unresolved");
    memory.fail_at=0;
    memory.put(db+0x38,std::uint32_t{0});
    check_identity(ModelIdentity::unresolved,"null name pointer unresolved");
    memory.put(db+0x38,name);
    memory.fail_at=name+3;
    check_identity(ModelIdentity::unresolved,"unreadable name bytes unresolved");
    memory.fail_at=0;
    memory.put(name,std::array<char,2>{char(0x80),0});
    check_identity(ModelIdentity::unresolved,"non-ASCII name unresolved");
    std::array<char,261> overlong{};
    overlong.fill('A'); overlong[260]=0;
    memory.put(name,overlong);
    check_identity(ModelIdentity::unresolved,"overlong name unresolved");
    need(capture.configure_plain_database_vtable(0),"disable name read between frames");
    memory.fail_at=instance+0x80;
    check_identity(ModelIdentity::unresolved,"default disabled model read preserves placement");
    need(result.objects[0].database==0 && result.objects[0].database_vtable==0,
         "disabled model read retains no database IDs");
    memory.fail_at=0;
    constexpr std::uint32_t craft_vtable=0x710000;
    need(capture.configure_plain_database_vtable(plain_vtable),"enable database identity for craft profile");
    need(capture.configure_craft_instance_vtable(craft_vtable),"configure craft instance profile");
    const std::array<float,12> rotated={0,1,0,-1,0,0,0,0,1,10,20,30};
    auto prep_craft=[&](std::uint32_t vtable,std::uint8_t active,const std::array<float,3>& scale) {
        fixture(memory); memory.put(instance+0x44,rotated); memory.put(instance, vtable);
        memory.put(instance+0x74,active); memory.put(instance+0x28,scale);
        memory.put(instance+0x80,db); memory.put(db,plain_vtable); memory.put(db+0x38,name);
        memory.put(name,filename);
    };
    prep_craft(craft_vtable,1,std::array<float,3>{2,-3,4});
    need(capture.begin(camera,true),"begin scaled craft frame");
    capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result),"scaled craft publishes");
    const auto& scaled=result.objects[0];
    need(scaled.placement_identity==PlacementIdentity::craft_runtime_scale &&
         scaled.instance_vtable==craft_vtable,"craft instance classified");
    close(scaled.runtime_scale[0],2,"scale x captured"); close(scaled.runtime_scale[1],-3,"scale y captured");
    close(scaled.runtime_scale[2],4,"scale z captured");
    const float scaled_expected[16]={0,3,0,10, 2,0,0,20, 0,0,4,30, 0,0,0,1};
    for(int i=0;i<16;++i)close(scaled.world[i],scaled_expected[i],"rotated translated scale columns");
    prep_craft(craft_vtable,0,std::array<float,3>{std::numeric_limits<float>::quiet_NaN(),0,0});
    memory.forbidden_at=instance+0x28;
    need(capture.begin(camera,true),"begin authored placement frame"); capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result),"inactive scale publishes authored placement");
    need(!memory.read_forbidden && result.objects[0].placement_identity==PlacementIdentity::craft_runtime_scale,
         "inactive scale ignores stale scale bytes");
    for(float scale:result.objects[0].runtime_scale)close(scale,1,"inactive scale remains identity");
    for(int i=0;i<16;++i)close(result.objects[0].world[i],expected[i],"inactive scale preserves placement");
    memory.forbidden_at=0;
    prep_craft(craft_vtable,1,std::array<float,3>{1,1,1});
    auto reject_craft=[&](std::uint32_t fail,const char* message) {
        memory.fail_at=fail; const Frame before=result;
        need(capture.begin(camera,true),"begin rejected craft frame"); capture.observe_post_frustum(entity,camera);
        need(!capture.end(camera,result),message); preserved(result,before,"craft failure preserves published output");
        memory.fail_at=0;
    };
    memory.fail_at=instance;
    { need(capture.begin(camera,true),"begin unreadable vtable"); capture.observe_post_frustum(entity,camera);
      need(capture.end(camera,result),"unreadable instance vtable publishes unresolved telemetry");
      need(result.objects[0].placement_identity==PlacementIdentity::unresolved,"unreadable instance vtable unresolved"); }
    memory.fail_at=0;
    reject_craft(instance+0x74,"unreadable scale flag drops frame");
    reject_craft(instance+0x28,"unreadable active scale drops frame");
    prep_craft(craft_vtable,1,std::array<float,3>{std::numeric_limits<float>::max(),2,3});
    { auto overflow_matrix=rotated; overflow_matrix[0]=2; memory.put(instance+0x44,overflow_matrix); }
    { const Frame before=result; need(capture.begin(camera,true),"begin overflow scale");
      capture.observe_post_frustum(entity,camera); need(!capture.end(camera,result),"scale multiplication overflow drops frame");
      preserved(result,before,"overflow scale preserves published output"); }
    prep_craft(craft_vtable,1,std::array<float,3>{1,0,1});
    { const Frame before=result; need(capture.begin(camera,true),"begin zero scale");
      capture.observe_post_frustum(entity,camera); need(!capture.end(camera,result),"zero scale drops frame");
      preserved(result,before,"zero scale preserves published output"); }
    for(const auto value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        prep_craft(craft_vtable,1,std::array<float,3>{1,value,1});const Frame before=result;
        need(capture.begin(camera,true),"begin nonfinite active scale");capture.observe_post_frustum(entity,camera);
        need(!capture.end(camera,result),"nonfinite active scale drops frame");preserved(result,before,"nonfinite scale preserves published output");
    }
    prep_craft(craft_vtable+4,1,std::array<float,3>{2,2,2});
    memory.forbidden_at=instance+0x74;
    need(capture.begin(camera,true),"begin unsupported instance"); capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result),"unsupported instance remains telemetry");
    need(result.objects[0].placement_identity==PlacementIdentity::unsupported_instance &&
         !memory.read_forbidden,"unknown type does not read scale flag"); memory.forbidden_at=0;
    prep_craft(craft_vtable,1,std::array<float,3>{2,2,2}); memory.put(db,std::uint32_t{plain_vtable+4});
    memory.forbidden_at=instance+0x74;
    need(capture.begin(camera,true),"begin non-plain database"); capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result),"non-plain database remains telemetry");
    need(result.objects[0].placement_identity==PlacementIdentity::unsupported_instance &&
         !memory.read_forbidden,"non-plain database skips scale reads"); memory.forbidden_at=0;
    prep_craft(craft_vtable,1,std::array<float,3>{2,2,2}); memory.fail_at=instance+0x80;
    need(capture.begin(camera,true),"begin unreadable database"); capture.observe_post_frustum(entity,camera);
    need(capture.end(camera,result),"unreadable database placement remains telemetry");
    need(result.objects[0].placement_identity==PlacementIdentity::unsupported_instance,
         "unreadable database cannot authorize craft scaling"); memory.fail_at=0;
    { const Frame before=result; need(capture.begin(camera,true),"begin configuration guard");
      need(!capture.configure_craft_instance_vtable(craft_vtable+4),"active craft profile reconfiguration rejected");
      capture.abort(); preserved(result,before,"configuration guard leaves published output intact"); }
    std::puts("PASS native11 scene capture ownership, matrix, camera, invalidation, capacity, model identity");
}
