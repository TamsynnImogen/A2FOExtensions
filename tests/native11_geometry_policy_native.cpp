// Offscreen public API10 geometry policy and i686 C ABI regression.
#include "../sdk/include/a2fo_native11_geometry_policy_api.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
extern "C" std::int32_t native11_c_query10(A2FO_N11_GetInterface,A2FO_N11_API10*);
extern "C" std::int32_t native11_c_submit_geometry(const A2FO_N11_API10*,A2FO_N11_Session,
    const A2FO_N11_Frame*);
static_assert(sizeof(A2FO_N11_API10)==88&&offsetof(A2FO_N11_API10,configure_geometry_policies)==80&&
    offsetof(A2FO_N11_API10,submit_world_native_geometry)==84,"i686 API10 append");
namespace {
void need(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(std::int32_t hr,const char* why){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,
    static_cast<unsigned long>(hr));std::exit(1);}}
void identity(float matrix[16]){for(int i=0;i<16;++i)matrix[i]=i%5==0?1.f:0.f;}
std::uint32_t bits(float v){std::uint32_t out=0;std::memcpy(&out,&v,4);return out;}
enum class Mode {valid,none,duplicate,short_policy,reserved,unknown_cw,negative,nan,inf};
struct Host {
    std::vector<std::uint8_t> sod=native11_fixture::model("",true);
    A2FO_N11_API10* api=nullptr;A2FO_N11_Session session=0;
    unsigned reads=0,model_reads=0,part_calls=0,policy_calls=0;
    Mode mode=Mode::valid;bool try_reentry=false,reentry_refused=false;
};
std::int32_t A2FO_N11_CALL asset(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context){
    auto& host=*static_cast<Host*>(raw);++host.reads;
    if(kind!=1||std::string(reinterpret_cast<const char*>(id),length)!="geometry-view"||
       host.sod.size()>limit)return 0;
    ++host.model_reads;return sink(context,host.sod.data(),static_cast<std::uint32_t>(host.sod.size()));
}
std::int32_t A2FO_N11_CALL part(void* raw,const std::uint8_t* id,std::uint32_t length,
    std::uint32_t ordinal,const float authored[16],A2FO_N11_PartStateSink sink,void* context){
    auto& host=*static_cast<Host*>(raw);++host.part_calls;
    need(ordinal==0&&std::string(reinterpret_cast<const char*>(id),length)=="geometry-view",
         "complete one-part model callback");
    A2FO_N11_PartState value{};value.struct_size=sizeof(value);value.visible=1;
    std::memcpy(value.local_to_model,authored,sizeof(value.local_to_model));
    value.local_to_model[3]=.25f; // Translation must remain .25 under mesh scale .5.
    return sink(context,&value);
}
std::int32_t A2FO_N11_CALL resolve(void* raw,const std::uint8_t* id,std::uint32_t length,
    A2FO_N11_GeometryPolicySink sink,void* context){
    auto& host=*static_cast<Host*>(raw);++host.policy_calls;
    need(std::string(reinterpret_cast<const char*>(id),length)=="geometry-view",
         "policy receives model identifier");
    if(host.try_reentry){
        host.try_reentry=false;
        A2FO_N11_GeometryPolicies same{sizeof(same),0,&host,resolve};
        host.reentry_refused=host.api->configure_geometry_policies(host.session,&same)==E_UNEXPECTED;
    }
    if(host.mode==Mode::none)return 1;
    A2FO_N11_GeometryPolicy value{sizeof(value),0,bits(.5f),0x10000u|0x037fu};
    switch(host.mode){
    case Mode::short_policy:--value.struct_size;break;
    case Mode::reserved:value.reserved=1;break;
    case Mode::unknown_cw:value.native_x87_control=0;break;
    case Mode::negative:value.parser_scalar_bits=bits(-.5f);break;
    case Mode::nan:value.parser_scalar_bits=bits(std::numeric_limits<float>::quiet_NaN());break;
    case Mode::inf:value.parser_scalar_bits=bits(std::numeric_limits<float>::infinity());break;
    default:break;
    }
    const auto result=sink(context,&value);
    if(host.mode==Mode::duplicate)sink(context,&value);
    return result;
}
std::int32_t A2FO_N11_CALL resolve_alt(void* raw,const std::uint8_t* id,std::uint32_t length,
    A2FO_N11_GeometryPolicySink sink,void* context){return resolve(raw,id,length,sink,context);}
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Readback read{};read.struct_size=sizeof(read);
    need(api.readback(session,&read)==S_FALSE&&read.width==32&&read.height==32&&
         read.required_bytes==32*32*4,"offscreen readback metadata");
    std::vector<std::uint8_t> data(read.required_bytes);read.bgra=data.data();read.capacity=data.size();
    ok(api.readback(session,&read),"offscreen readback");return data;
}
int bright(const std::vector<std::uint8_t>& image,unsigned x){
    const std::size_t offset=(16*32+x)*4;
    return int(image[offset])+int(image[offset+1])+int(image[offset+2]);
}
A2FO_N11_Usage usage(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Usage result{};result.struct_size=sizeof(result);
    ok(api.usage(session,&result),"GPU usage");return result;
}
bool same_usage(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b){
    return a.device_epoch==b.device_epoch&&a.mesh_bytes==b.mesh_bytes&&
        a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"renderer DLL loaded");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API10 api10{};ok(native11_c_query10(get,&api10),"C API10 query");
    auto& api=api10.base.base.base.base.base.base.base.base.base;
    need(api.version==10&&api.struct_size==88&&api.reserved==0&&
         (api.capabilities&A2FO_N11_CAP_NATIVE_GEOMETRY)&&
         api10.configure_geometry_policies&&api10.submit_world_native_geometry,
         "API10 header and appended functions");
    const std::array<std::uint32_t,9> sizes{{40,48,52,56,60,64,68,76,80}};
    for(std::uint32_t version=1;version<=9;++version){
        std::array<std::uint8_t,80> table{};
        ok(get(version,sizes[version-1],table.data()),"legacy API query");
        std::uint32_t size=0,reported_version=0;
        std::memcpy(&size,table.data(),4);std::memcpy(&reported_version,table.data()+4,4);
        need(size==sizes[version-1]&&reported_version==version,"API1-9 prefix sizes retained");
    }
    A2FO_N11_API10 bad;std::memset(&bad,0x5a,sizeof(bad));const auto sentinel=bad;
    need(get(10,sizeof(bad)-1,&bad)==E_INVALIDARG&&
         std::memcmp(&bad,&sentinel,sizeof(bad))==0,"short query leaves output untouched");
    need(get(UINT32_MAX,sizeof(bad),&bad)==E_NOINTERFACE&&
         std::memcmp(&bad,&sentinel,sizeof(bad))==0,"unknown version leaves output untouched");
    Host host;host.api=&api10;A2FO_N11_Host request{sizeof(request),0,32,32,&host,asset,nullptr};
    ok(api.create(&request,&host.session),"offscreen session");
    const auto empty_usage=usage(api,host.session); // Includes renderer-owned white texture.
    std::string name="geometry-view";A2FO_N11_Object object{};identity(object.local_to_world);
    object.model_identifier=reinterpret_cast<const std::uint8_t*>(name.data());
    object.model_identifier_length=static_cast<std::uint32_t>(name.size());
    A2FO_N11_Frame frame{};frame.struct_size=sizeof(frame);frame.generation=frame.source_epoch=1;
    frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS|A2FO_N11_FRAME_REQUIRE_PART_STATES;
    identity(frame.world_to_clip);frame.viewport[2]=frame.viewport[3]=32;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    frame.object_count=1;frame.objects=&object;
    need(native11_c_submit_geometry(&api10,host.session,&frame)==E_INVALIDARG&&host.reads==0,
         "native geometry rejects absent part and policy resolvers before asset read");
    A2FO_N11_PartStates states{sizeof(states),0,&host,part};
    ok(api10.base.base.base.base.base.base.configure_part_states(host.session,&states),"part resolver");
    need(native11_c_submit_geometry(&api10,host.session,&frame)==E_INVALIDARG&&host.reads==0,
         "native geometry rejects absent policy resolver before asset read");
    A2FO_N11_GeometryPolicies policies{sizeof(policies),0,&host,resolve};
    ok(api10.configure_geometry_policies(host.session,&policies),"policy resolver");
    frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS;
    need(native11_c_submit_geometry(&api10,host.session,&frame)==E_INVALIDARG&&host.reads==0,
         "native geometry requires all part state flag");
    frame.flags=A2FO_N11_FRAME_REQUIRE_PART_STATES;
    need(native11_c_submit_geometry(&api10,host.session,&frame)==E_INVALIDARG&&host.reads==0,
         "native geometry requires complete casters flag");
    frame.flags|=A2FO_N11_FRAME_COMPLETE_CASTERS;
    A2FO_N11_GeometryPolicies invalid=policies;invalid.reserved=1;
    need(api10.configure_geometry_policies(host.session,&invalid)==E_INVALIDARG,
         "reserved config rejected");
    invalid=policies;--invalid.struct_size;
    need(api10.configure_geometry_policies(host.session,&invalid)==E_INVALIDARG,
         "short config rejected");
    host.try_reentry=true;
    ok(native11_c_submit_geometry(&api10,host.session,&frame),"native geometry frame");
    need(host.reentry_refused&&host.policy_calls==1&&host.model_reads==1&&host.part_calls==1,
         "one policy and model read; callback reentry refused");
    auto native_image=pixels(api,host.session);auto native_usage=usage(api,host.session);
    need(bright(native_image,15)<30&&bright(native_image,20)>100&&native_usage.meshes==1,
         "half-scale footprint and unchanged quarter-unit node translation");
    const auto hot_reads=host.reads,hot_policy=host.policy_calls;
    ++frame.generation;ok(native11_c_submit_geometry(&api10,host.session,&frame),"hot native frame");
    need(host.reads==hot_reads&&host.policy_calls==hot_policy&&
         same_usage(native_usage,usage(api,host.session))&&pixels(api,host.session)==native_image,
         "hot frame reuses metadata, geometry and pixels");
    ++frame.source_epoch;++frame.generation;
    ok(native11_c_submit_geometry(&api10,host.session,&frame),"new source epoch");
    need(host.model_reads==2&&host.policy_calls==2,"epoch reloads policy and model");
    native_image=pixels(api,host.session);native_usage=usage(api,host.session);
    A2FO_N11_GeometryPolicies alternative{sizeof(alternative),0,&host,resolve_alt};
    ok(api10.configure_geometry_policies(host.session,&alternative),"replace callback");
    ++frame.generation;ok(native11_c_submit_geometry(&api10,host.session,&frame),"replacement callback frame");
    need(host.model_reads==3&&host.policy_calls==3,"callback replacement clears geometry cache");
    native_image=pixels(api,host.session);native_usage=usage(api,host.session);
    for(auto mode:{Mode::none,Mode::duplicate,Mode::short_policy,Mode::reserved,
                   Mode::unknown_cw,Mode::negative,Mode::nan,Mode::inf}){
        host.mode=mode;++frame.source_epoch;++frame.generation;
        const auto failed=native11_c_submit_geometry(&api10,host.session,&frame);
        const auto resident=usage(api,host.session);
        const bool same_image=pixels(api,host.session)==native_image;
        if(!(failed<0&&same_image&&resident.meshes==empty_usage.meshes&&resident.textures==empty_usage.textures))
            std::fprintf(stderr,"policy mode=%u HRESULT=%08lx same_image=%u meshes=%u textures=%u\n",
                static_cast<unsigned>(mode),static_cast<unsigned long>(failed),same_image,resident.meshes,resident.textures);
        need(failed<0&&same_image&&resident.meshes==empty_usage.meshes&&resident.textures==empty_usage.textures,
             "invalid policy fails without new GPU resource commit or image change");
    }
    host.mode=Mode::valid;const auto failed_calls=host.policy_calls;++frame.generation;
    need(native11_c_submit_geometry(&api10,host.session,&frame)<0&&
         host.policy_calls==failed_calls,"failed model cached within source epoch");
    ++frame.source_epoch;++frame.generation;
    ok(native11_c_submit_geometry(&api10,host.session,&frame),"recover after policy failures");
    need(pixels(api,host.session)==native_image,"recovered native image");
    ++frame.generation;ok(api10.base.submit_world_all_parts(host.session,&frame),"API9 legacy scope");
    auto legacy_image=pixels(api,host.session);
    need(bright(legacy_image,15)>100&&bright(legacy_image,20)>100&&
         legacy_image!=native_image,"API9 restores unscaled footprint");
    ++frame.generation;ok(native11_c_submit_geometry(&api10,host.session,&frame),"return to API10 scope");
    need(pixels(api,host.session)==native_image,"API10 restores scaled footprint");
    ok(api.destroy(host.session),"destroy session");need(FreeLibrary(dll)!=0,"DLL unload");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path required");
    run(argv[1]);std::puts("PASS native11 geometry policy native");}
