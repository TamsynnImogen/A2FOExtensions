// Copied class ODF -> pure world lights -> public v4 offscreen renderer.
#include "../renderer/native11/light_event.hpp"
#include "../renderer/native11/world_lighting.hpp"
#include "../renderer/native11/lighting_service.hpp"
#include <memory>
#include "../sdk/include/a2fo_native11_api.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(std::int32_t hr,const char* why){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
unsigned admission_configs=0;
std::int32_t A2FO_N11_CALL counted_config(A2FO_N11_Session,const A2FO_N11_Shadows*){
    ++admission_configs;return S_OK;
}
struct Host {std::vector<std::uint8_t> sod=native11_fixture::model();unsigned reads=0;bool block=false;};
std::int32_t A2FO_N11_CALL read_asset(void* context,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t size,std::uint32_t limit,A2FO_N11_BytesSink sink,void* sink_context){
    auto& host=*static_cast<Host*>(context);need(!host.block,"hot frame has no content reads");++host.reads;
    const char name[]="world-light-model";
    if(kind!=1||size!=sizeof(name)-1||std::memcmp(id,name,size)||host.sod.size()>limit)return 0;
    return sink(sink_context,host.sod.data(),static_cast<std::uint32_t>(host.sod.size()));
}
struct Event {
    std::array<std::string,6> names{{"lightSource","lightType","lightColor","lightDirection",
        "lightIntensity","lightCastShadows"}};
    std::array<std::string,6> values{{"1","directional","1 0 0","0.5 0 1","0.6","1"}};
    std::array<A2FO_OdfFieldView,6> fields{};
    A2FO_GameObjectClassLoadedEvent event{};
    Event(){for(std::size_t i=0;i<fields.size();++i){fields[i].name={names[i].data(),static_cast<std::uint32_t>(names[i].size())};
            fields[i].value={values[i].data(),static_cast<std::uint32_t>(values[i].size())};}
        event.struct_size=sizeof(event);event.object_class=this;event.odf_fields=fields.data();
        event.odf_field_count=static_cast<std::uint32_t>(fields.size());}
};
void identity(float m[16]){for(int i=0;i<16;++i)m[i]=0;for(int i=0;i<4;++i)m[i*5]=1;}
struct Scene {std::array<A2FO_N11_Object,2> objects{};std::array<A2FO_N11_Light,8> lights{};
    A2FO_N11_Frame frame{};const std::string name="world-light-model";
    Scene(){for(auto& object:objects){identity(object.local_to_world);
            object.model_identifier=reinterpret_cast<const std::uint8_t*>(name.data());
            object.model_identifier_length=static_cast<std::uint32_t>(name.size());}
        objects[0].local_to_world[11]=.6f;
        for(int i:{0,5,10})objects[1].local_to_world[i]=.2f;
        objects[1].local_to_world[11]=.26f;
        frame.struct_size=sizeof(frame);frame.generation=frame.source_epoch=1;
        identity(frame.world_to_clip);frame.viewport[2]=frame.viewport[3]=32;
        frame.object_count=2;frame.objects=objects.data();}
    void lighting(const LightingFrame& world){
        need(world.lights.size()<=lights.size(),"light budget");
        frame.light_count=static_cast<std::uint32_t>(world.lights.size());
        frame.lights=frame.light_count?lights.data():nullptr;
        for(int c=0;c<3;++c)frame.ambient[c]=world.ambient[c];
        for(std::size_t i=0;i<world.lights.size();++i){const auto& a=world.lights[i];auto& b=lights[i];
            b.type=static_cast<std::uint32_t>(a.type);
            for(int c=0;c<3;++c){b.position[c]=a.position[c];b.direction[c]=a.direction[c];b.colour[c]=a.colour[c];}
            b.intensity=a.intensity;b.falloff_start=a.falloff_start;b.falloff_range=a.falloff_range;
            b.cone_inner_degrees=a.cone_inner_degrees;b.cone_outer_degrees=a.cone_outer_degrees;}
    }
};
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Readback read{};read.struct_size=sizeof(read);
    need(api.readback(session,&read)==S_FALSE&&read.width==32&&read.height==32&&read.required_bytes==4096,
        "readback metadata");
    std::vector<std::uint8_t> out(static_cast<std::size_t>(read.required_bytes));
    read.bgra=out.data();read.capacity=out.size();ok(api.readback(session,&read),"readback pixels");return out;
}
int channel(const std::vector<std::uint8_t>& bytes,unsigned x,unsigned y,unsigned c){return bytes[(y*32+x)*4+c];}
void modules(){HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
    need(snapshot!=INVALID_HANDLE_VALUE,"module snapshot");MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
    need(Module32FirstW(snapshot,&entry)!=0,"module enumeration starts");do{
        need(_wcsicmp(entry.szModule,L"d3d8.dll")&&_wcsicmp(entry.szModule,L"d3d9.dll")&&
             _wcsnicmp(entry.szModule,L"d3dx",4),"no legacy Direct3D modules");
    }while(Module32NextW(snapshot,&entry));const DWORD last=GetLastError();CloseHandle(snapshot);
    need(last==ERROR_NO_MORE_FILES,"module enumeration completes");}
void run(const char* path){
    Event copied;LightConfig config;std::string error;
    need(copy_light_config(copied.event,config,error)&&config.enabled&&config.cast_shadows,
        "copied class light configuration");
    for(auto& value:copied.values)value.assign(value.size(),'x');
    need(config.light.colour[0]==1&&config.light.colour[1]==0&&config.light.intensity==.6f,
        "event buffers do not own parsed config");
    PlacedLight red{};red.source_id=1;red.config=config;
    const float ambient[3]{.1f,.1f,.1f};LightingFrame world;
    auto assemble=[&](const std::vector<PlacedLight>& sources){
        need(assemble_world_lighting(sources,true,ambient,world,error),"complete world light assembly");};
    assemble({red});need(world.lights.size()==1&&world.source_ids==std::vector<std::uint64_t>{1}&&
        world.shadows.light_mask==1,"red source shadow index");
    HMODULE dll=LoadLibraryA(path);need(dll,"renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"public interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API4 api4{};ok(get(4,sizeof(api4),&api4),"API4 query");
    need(api4.base.base.base.version==4&&api4.base.base.configure_shadows&&
         api4.base.base.base.submit,"API4 service and shadow prefix");
    auto& api=api4.base.base.base;
    Host host;A2FO_N11_Host request{sizeof(request),0,32,32,&host,read_asset,nullptr};
    A2FO_N11_Session session=0;ok(api.create(&request,&session),"offscreen session");
    Scene scene;scene.lighting(world);
    A2FO_N11_Shadows shadows{sizeof(shadows),world.shadows.light_mask,256,.001f};
    auto owned=std::make_unique<Frame>();
    owned->camera=1;owned->generation=1;
    auto malformed=api4;malformed.base.base.configure_shadows=counted_config;
    for(const auto cap:{A2FO_N11_CAP_OFFSCREEN_STATIC,A2FO_N11_CAP_TEXTURE_BINDINGS,A2FO_N11_CAP_PART_STATES}){
        malformed.base.base.base.capabilities=api.capabilities&~cap;
        need(submit_lit_capture(malformed,session,*owned,1,world)==E_INVALIDARG&&admission_configs==0,
            "malformed API rejected before shadow configuration");
    }
    malformed.base.base.base.capabilities=api.capabilities;owned->object_count=1;
    need(submit_lit_capture(malformed,session,*owned,1,world)==E_INVALIDARG&&admission_configs==0,
        "unresolved frame rejected before shadow configuration");
    owned->object_count=0;
    auto submit_world=[&](){
        owned->camera=1;owned->generation=scene.frame.generation;
        owned->object_count=scene.frame.object_count;
        std::memcpy(owned->view.world_to_clip,scene.frame.world_to_clip,sizeof(owned->view.world_to_clip));
        std::memcpy(owned->view.viewport,scene.frame.viewport,sizeof(owned->view.viewport));
        for(std::size_t i=0;i<owned->object_count;++i){auto& out=owned->objects[i];const auto& in=scene.objects[i];
            out.model_identity=ModelIdentity::plain_database_name;
            out.placement_identity=PlacementIdentity::authored_placement;
            std::memcpy(out.world,in.local_to_world,sizeof(out.world));
            std::memcpy(out.model_name,in.model_identifier,in.model_identifier_length);
            out.model_name[in.model_identifier_length]=0;
        }
        auto selected=world;selected.shadows.light_mask=shadows.light_mask;
        selected.shadows.resolution=shadows.resolution;selected.shadows.depth_bias=shadows.depth_bias;
        return submit_lit_capture(api4,session,*owned,scene.frame.source_epoch,selected);
    };
    ok(submit_world(),"red shadow frame");auto positive=pixels(api,session);
    std::printf("ODF directional receiver red: shadow=%d clear=%d\n",channel(positive,20,16,2),channel(positive,11,16,2));
    need(host.reads==1,"model content read once");
    need(channel(positive,20,16,2)>15&&channel(positive,20,16,2)<
         channel(positive,11,16,2),"directional red shadow darkens receiver outside visible blocker");
    host.block=true;
    red.config.light.direction[0]=-.5f;assemble({red});scene.lighting(world);
    ok(submit_world(),"opposite ray frame");auto negative=pixels(api,session);
    need(channel(negative,11,16,2)<channel(negative,20,16,2)&&host.reads==1,
        "opposite ray moves shadow without content reread");
    red.config.light.intensity=0;assemble({red});scene.lighting(world);
    shadows.light_mask=world.shadows.light_mask;ok(api4.base.base.configure_shadows(session,&shadows),"zero energy removes shadow");
    ok(submit_world(),"ambient-only frame");auto unlit=pixels(api,session);
    need(world.lights.empty()&&world.shadows.light_mask==0&&channel(unlit,20,16,2)>15&&
         channel(unlit,20,16,2)<40,"zero energy keeps ambient");
    red.config.light.intensity=.6f;red.config.light.direction[0]=.5f;
    red.config.light.colour[0]=0;red.config.light.colour[1]=1;
    assemble({red});scene.lighting(world);shadows.light_mask=0;
    ok(api4.base.base.configure_shadows(session,&shadows),"unshadowed green settings");
    ok(submit_world(),"green hue frame");auto green=pixels(api,session);
    need(channel(green,11,16,1)>channel(green,11,16,2)+30&&
         channel(positive,11,16,2)>channel(positive,11,16,1)+30,"ODF colour changes native hue");
    red.config.light.intensity=.3f;assemble({red});scene.lighting(world);
    ok(submit_world(),"half-intensity green");auto dim=pixels(api,session);
    need(channel(dim,11,16,1)+30<channel(green,11,16,1)&&channel(dim,11,16,2)==channel(green,11,16,2),
        "intensity changes direct energy and preserves other channels");
    red.config.light.intensity=.6f;
    red.config.light.colour[0]=1;red.config.light.colour[1]=0;
    PlacedLight second=red;second.source_id=2;second.config.cast_shadows=false;
    second.config.light.colour[0]=0;second.config.light.colour[1]=1;
    assemble({red,second});scene.lighting(world);
    need(world.shadows.light_mask==1&&world.source_ids==std::vector<std::uint64_t>({1,2}),
        "only red source casts");
    shadows.light_mask=world.shadows.light_mask;ok(api4.base.base.configure_shadows(session,&shadows),"per-source mask");
    ok(submit_world(),"red and green frame");auto mixed=pixels(api,session);
    need(channel(mixed,20,16,2)<channel(mixed,11,16,2)&&
         channel(mixed,20,16,1)>channel(mixed,20,16,2)+30,"red occlusion preserves green");
    PlacedLight point=red;point.config.cast_shadows=false;point.config.light.type=LightType::point;
    point.config.light.falloff_start=0;point.config.light.falloff_range=3;
    point.local_to_world[3]=-.5f;assemble({point});scene.lighting(world);shadows.light_mask=0;
    ok(api4.base.base.configure_shadows(session,&shadows),"point settings");
    ok(submit_world(),"point left");auto point_left=pixels(api,session);
    point.local_to_world[3]=.5f;assemble({point});scene.lighting(world);
    need(world.lights[0].position[0]==.5f,"point world translation");
    ok(submit_world(),"point right");auto point_right=pixels(api,session);
    need(point_left!=point_right,"point translation changes native pixels");
    point.config.light.falloff_range=.2f;assemble({point});scene.lighting(world);
    ok(submit_world(),"short point range");auto short_range=pixels(api,session);
    need(short_range!=point_right,"falloff range changes local illumination");
    PlacedLight spot=red;spot.config.cast_shadows=false;spot.config.light.type=LightType::spot;
    spot.config.light.direction[0]=.5f;spot.config.light.direction[1]=0;spot.config.light.direction[2]=1;
    spot.config.light.falloff_start=0;spot.config.light.falloff_range=3;
    spot.config.light.cone_inner_degrees=30;spot.config.light.cone_outer_degrees=60;
    spot.local_to_world[3]=-.5f;assemble({spot});scene.lighting(world);
    ok(submit_world(),"spot original");auto spot_left=pixels(api,session);
    spot.local_to_world[0]=-1;spot.local_to_world[10]=-1;
    assemble({spot});scene.lighting(world);
    need(world.lights[0].direction[0]<0,"spot world rotation");
    ok(submit_world(),"spot rotated");auto spot_right=pixels(api,session);
    need(spot_left!=spot_right,"spot rotation changes native pixels");
    spot.local_to_world[0]=spot.local_to_world[10]=1;
    spot.config.light.cone_inner_degrees=1;spot.config.light.cone_outer_degrees=2;
    assemble({spot});scene.lighting(world);ok(submit_world(),"narrow cone");
    need(pixels(api,session)!=spot_left,"spot cone cutoff changes native pixels");
    need(host.reads==1,"light-only frames preserve content cache");
    ok(api.destroy(session),"session destroy");modules();need(FreeLibrary(dll)!=0,"DLL unload");
}
}
int main(int argc,char** argv){need(argc==2,"usage: native11_world_lighting_native.exe RENDERER_DLL");
    run(argv[1]);std::puts("PASS native11 copied ODF world lighting public API4");}
