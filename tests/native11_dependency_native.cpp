#include "../renderer/native11/dependency_plan.hpp"
#include "../renderer/native11/capture_service.hpp"
#include "../renderer/native11/texture_store.hpp"
#include "native11_model_fixture.hpp"
#include "../rust/native11_model_ffi.h"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool value,const char* label) {
    if(!value){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",label,GetLastError());std::exit(1);}
}
void ok(std::int32_t hr,const char* label) {
    if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,static_cast<unsigned long>(hr));std::exit(1);}
}
unsigned parser_calls=0;
int counted_parser(const std::uint8_t* bytes,std::uint32_t count,A2FO_N11_PartSink sink,void* context) {
    ++parser_calls;
    return a2fo_rs_native11_parse_sod(bytes,count,sink,context);
}
OpenedFile file(std::string name,std::vector<std::uint8_t> bytes) {
    OpenedFile out;out.name=std::move(name);out.bytes=std::move(bytes);return out;
}
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial) {
    return static_cast<ModelAssetStore*>(raw)->find(db,serial);
}
std::vector<std::uint8_t> tga(bool red) {
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;bytes[16]=32;bytes[17]=0x28;
    bytes[red?20:19]=255;bytes[21]=255;return bytes;
}
std::unique_ptr<Frame> frame(const ModelAssetInfo& left,const ModelAssetInfo& right) {
    auto out=std::make_unique<Frame>();out->camera=1;out->generation=1;out->object_count=2;
    for(unsigned i=0;i<4;++i)out->view.world_to_clip[i*5]=1;
    out->view.viewport[2]=out->view.viewport[3]=128;
    for(unsigned i=0;i<2;++i){auto& object=out->objects[i];const auto& model=i?right:left;
        object.database=model.database;object.model_serial=model.serial;
        object.model_identity=ModelIdentity::plain_database_name;
        object.placement_identity=PlacementIdentity::authored_placement;
        std::strcpy(object.model_name,model.name);
        for(unsigned j=0;j<4;++j)object.world[j*5]=1;
        object.world[3]=i?.5f:-.5f;
    }
    return out;
}
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session) {
    A2FO_N11_Readback output{};output.struct_size=sizeof(output);
    need(api.readback(session,&output)==S_FALSE&&output.width==128&&output.height==128&&
         output.required_bytes==128*128*4,"readback metadata");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(output.required_bytes));
    output.bgra=bytes.data();output.capacity=bytes.size();ok(api.readback(session,&output),"readback pixels");
    return bytes;
}
void verify_colours(const std::vector<std::uint8_t>& image) {
    const auto* left=image.data()+(64*128+32)*4;const auto* right=image.data()+(64*128+96)*4;
    const auto red=[](const std::uint8_t* p){return p[2]>250&&p[1]<4&&p[0]<4;};
    const auto green=[](const std::uint8_t* p){return p[1]>250&&p[2]<4&&p[0]<4;};
    need(red(left)&&green(right),"left red and right green pixels");
}
void verify_no_legacy_modules() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    need(snapshot!=INVALID_HANDLE_VALUE,"module snapshot");
    MODULEENTRY32 entry{};entry.dwSize=sizeof(entry);
    bool legacy=false;
    need(Module32First(snapshot,&entry)!=0,"module enumeration begins");
    do {
        const char* name=entry.szModule;
        if(_strnicmp(name,"d3dx",4)==0||_stricmp(name,"d3d8.dll")==0||_stricmp(name,"d3d9.dll")==0)
            legacy=true;
    } while(Module32Next(snapshot,&entry));
    const auto enumeration_error=GetLastError();
    CloseHandle(snapshot);
    need(enumeration_error==ERROR_NO_MORE_FILES,"module enumeration completes");
    need(!legacy,"legacy Direct3D 8/9 and D3DX modules remain unloaded");
}
struct Host { const BoundCapture* capture=nullptr;unsigned reads=0; };
std::int32_t A2FO_N11_CALL read_asset(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context) {
    auto& host=*static_cast<Host*>(raw);need(host.capture!=nullptr,"bound callback owner");++host.reads;
    return BoundCapture::read_asset(const_cast<BoundCapture*>(host.capture),kind,id,length,limit,sink,context);
}
void check_optional_sod(const char* path,std::uint32_t database,ModelDependencyCache& cache,
                        std::uint32_t expected_parts,const char* expected_texture) {
    std::ifstream input(path,std::ios::binary);need(input.good(),"optional SOD opens");
    input.seekg(0,std::ios::end);const auto end=input.tellg();
    need(end>0&&static_cast<std::uint64_t>(end)<=64u*1024u*1024u,"optional SOD bounded size");
    input.seekg(0,std::ios::beg);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    need(input.gcount()==static_cast<std::streamsize>(bytes.size()),"optional SOD complete read");
    auto model=std::make_shared<ModelAsset>();model->info.serial=database;model->info.database=database;
    model->info.byte_count=static_cast<std::uint32_t>(bytes.size());model->bytes=std::move(bytes);
    const auto slash=std::strrchr(path,'/');const auto backslash=std::strrchr(path,'\\');
    const auto* base=slash?slash+1:path;if(backslash&&backslash+1>base)base=backslash+1;
    const auto name_size=std::min<std::size_t>(std::strlen(base),sizeof(model->info.name)-1);
    std::memcpy(model->info.name,base,name_size);
    std::shared_ptr<const ModelDependencies> result;std::string error;
    need(cache.get(model,&counted_parser,result,error),"optional real SOD parse");
    need(result->part_count==expected_parts&&result->textures==std::vector<std::string>{expected_texture},
         "real SOD exact static dependencies");
    std::printf("SOD %s: parts=%u textures=%zu",path,result->part_count,result->textures.size());
    for(const auto& texture:result->textures)std::printf(" %s",texture.c_str());
    std::puts("");
}
}

int main(int argc,char** argv) {
    need(argc==2||argc==4,"usage: native11_dependency_native.exe HELPER_DLL [VENTURE_SOD MED_SOD]");
    HMODULE dll=LoadLibraryA(argv[1]);need(dll!=nullptr,"helper renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"public C API export");
    A2FO_N11_GetInterface get=nullptr;static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API api{};ok(get(A2FO_N11_API_VERSION,sizeof(api),&api),"version 1 API");
    need(api.version==A2FO_N11_API_VERSION&&api.submit,"public API table");

    ModelAssetStore models;
    need(models.publish(1,0,file("refred.sod",native11_fixture::model("red.tga")))&&
         models.publish(2,0,file("refred.sod",native11_fixture::model("green.tga"))),
         "same filename models published");
    ModelAssetInfo left{},right{};
    need(models.info(1,left)&&models.info(2,right)&&left.serial!=right.serial,"independent model versions");
    auto original_frame=frame(left,right);
    ModelDependencyCache cache;parser_calls=0;
    std::shared_ptr<const DependencyPlan> plan;std::string error;
    need(build_dependency_plan(*original_frame,resolve,&models,cache,&counted_parser,plan,error),"first dependency plan");
    need(plan->models().size()==2&&plan->textures()==std::vector<std::string>({"red.tga","green.tga"})&&
         parser_calls==2,"two models and two ordered texture dependencies");
    std::shared_ptr<const DependencyPlan> second_plan;
    need(build_dependency_plan(*original_frame,resolve,&models,cache,&counted_parser,second_plan,error)&&
         parser_calls==2,"second plan hits both manifests without Rust parser calls");

    ResolvedTextureStore textures;
    need(textures.publish(1,"red.tga","fixture-red",0,false,tga(true))&&
         textures.publish(2,"green.tga","fixture-green",0,false,tga(false)),"required TGA textures");
    for(unsigned i=0;i<270;++i){
        const auto logical="junk-"+std::to_string(i);
        const bool colour=i==0,surface=i==1;
        need(textures.publish(10+i,logical,"junk",surface?4u:0u,colour,{1,2,3}),"unrelated registry entry");
    }
    need(textures.publish(400,"junk-duplicate","one",0,false,{1})&&
         textures.publish(401,"junk-duplicate","two",0,false,{2})&&
         textures.publish(402,"unused-colour","colour",0,true,{3})&&
         textures.publish(403,"unused-surface","surface",4,false,{4}),"unrelated invalid and ambiguous entries");
    std::vector<SnapshotTexture> collected{{"sentinel",std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{9})}};
    need(!textures.collect(collected,error)&&collected.size()==1&&collected[0].identifier=="sentinel",
         "all-assets collection fails atomically on unrelated registry contents");
    need(textures.collect_required(plan->textures(),collected,error)&&collected.size()==2&&
         collected[0].identifier=="green.tga"&&collected[1].identifier=="red.tga",
         "required collection returns only the two exact dependencies");

    auto bind_textures=[&](const std::vector<std::string>& requested,ResolvedTextureStore& source,
                           std::vector<SnapshotTexture>& output){return source.collect_required(requested,output,error);};
    std::vector<SnapshotTexture> atomic{{"sentinel",std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{7})}};
    need(!bind_textures({"red.tga","missing.tga"},textures,atomic)&&atomic.size()==1&&
         atomic[0].identifier=="sentinel","selected missing name rejects atomically");
    ResolvedTextureStore selected_colour;
    need(selected_colour.publish(1,"red.tga","bad",0,true,{1})&&
         !bind_textures({"red.tga"},selected_colour,atomic)&&atomic.size()==1&&atomic[0].identifier=="sentinel",
         "selected colour-key name rejects atomically");
    ResolvedTextureStore selected_conflict;
    need(selected_conflict.publish(1,"red.tga","one",0,false,{1})&&
         selected_conflict.publish(2,"red.tga","two",0,false,{2})&&
         !bind_textures({"red.tga"},selected_conflict,atomic)&&atomic.size()==1&&atomic[0].identifier=="sentinel",
         "selected conflicting duplicate name rejects atomically");

    std::shared_ptr<const BoundCapture> bound;
    need(build_bound_capture(plan->frame(),9,DependencyPlan::resolve,
         const_cast<DependencyPlan*>(plan.get()),collected,bound,error),"build bound capture from plan");
    need(bound->models().size()==2&&bound->model_asset("n11:model:"+std::to_string(left.serial))&&
         bound->model_asset("n11:model:"+std::to_string(right.serial)),"bound model identities remain serial distinct");
    models.clear();textures.clear();original_frame.reset();plan.reset();second_plan.reset();collected.clear();

    Host host{bound.get()};A2FO_N11_Host config{sizeof(config),0,128,128,&host,&read_asset,nullptr};
    A2FO_N11_Session session=0;ok(api.create(&config,&session),"offscreen public C API create");
    const float ambient[3]{1,1,1};
    ok(submit_captured(api,session,bound->frame(),bound->source_epoch(),nullptr,0,ambient),"public C API submit");
    verify_colours(pixels(api,session));
    A2FO_N11_Usage usage{};usage.struct_size=sizeof(usage);ok(api.usage(session,&usage),"resource usage");
    need(usage.meshes==2&&usage.textures==3,"two meshes, two diffuse textures, implicit white texture");
    verify_no_legacy_modules();
    ok(api.destroy(session),"public API session destroy");need(FreeLibrary(dll)!=0,"renderer helper DLL unload");

    ModelAssetStore untextured_models;
    need(untextured_models.publish(70,0,file("plain.sod",native11_fixture::model(""))),"untextured model publish");
    ModelAssetInfo plain{};need(untextured_models.info(70,plain),"untextured model metadata");
    auto plain_frame=std::make_unique<Frame>();plain_frame->object_count=1;
    auto& object=plain_frame->objects[0];object.database=70;object.model_serial=plain.serial;
    object.model_identity=ModelIdentity::plain_database_name;object.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(object.model_name,"plain.sod");
    std::shared_ptr<const DependencyPlan> plain_plan;
    need(build_dependency_plan(*plain_frame,resolve,&untextured_models,cache,&counted_parser,plain_plan,error)&&
         plain_plan->textures().empty(),"untextured dependency plan has no requested names");
    ResolvedTextureStore junk_registry;
    need(junk_registry.publish(1,"ignored-colour","junk",0,true,{1})&&
         junk_registry.publish(2,"ignored-surface","junk",4,false,{2}),"untextured unrelated registry setup");
    std::vector<SnapshotTexture> empty{{"sentinel",std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{8})}};
    need(junk_registry.collect_required(plain_plan->textures(),empty,error)&&empty.empty(),
         "untextured required collection ignores unsupported junk and returns empty");
    plain_frame.reset();plain_plan.reset();untextured_models.clear();junk_registry.clear();

    if(argc==4){ModelDependencyCache fixture_cache;
        check_optional_sod(argv[2],80,fixture_cache,1,"federation_venture");
        check_optional_sod(argv[3],81,fixture_cache,3,"federation_mediterranean");}
    std::puts("PASS native11 dependency plan, required textures, Rust parser cache, helper DLL pixels");
}
