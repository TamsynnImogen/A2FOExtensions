#include "refit_scene.hpp"
#include "native11_capture.hpp"
#include "native11_materials.hpp"
#include "native11_assets.hpp"
#include "native11_textures.hpp"
#include "extension_roots.hpp"
#include "../renderer/refit/startup.hpp"
#include "../renderer/refit/packet.hpp"
#include "../renderer/refit/resources.hpp"
#include "../renderer/refit/mesh_scope.hpp"
#include "../renderer/refit/geometry_parity.hpp"
#include "../renderer/refit/material_colours.hpp"
#include "../renderer/refit/rhw_workspace.hpp"
#include "refit_material_colours_profile.hpp"
#include "../renderer/refit/lights.hpp"
#include "../renderer/refit/view_scope.hpp"
#include "../renderer/native11/model_geometry_policy.hpp"
#include "../rust/native11_model_ffi.h"
#include "../rust/native11_group_material_ffi.h"
#include "refit_scene_profile.hpp"
#include "nebula_renderer.hpp"
#include <set>
#include "refit_mesh_capture.hpp"
#include "refit_scene_test.hpp"
#include "refit_upstream_sites.hpp"
#include <algorithm>
#include <cstdio>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace a2fo::refit_scene {
namespace {
struct Scope {
    void* instance=nullptr;void* camera=nullptr;
    std::shared_ptr<const native11::MaterialObservation> observation;
    native11::Object object{};
};
struct GeometrySource {
    std::shared_ptr<const native11::ModelAsset> model;
    std::vector<refit::AuthoredTriangle2> triangles;
    std::string material_name;
    A2FO_N11_GroupMaterial material{}; // name pointer is cleared after copying
};
// One authored face group of a native mesh node and its prepared packet. The
// native legacy body issues one checked opaque draw per group record, in group
// order; a plan is consumed only by the draw at its own index and must still
// pass the exact material and geometry-parity rechecks there.
struct GroupPlan {
    std::shared_ptr<const refit::Packet2> packet;
    std::shared_ptr<const native11::MaterialObservation> observation;
    std::shared_ptr<const GeometrySource> geometry;
    std::uint32_t triangles=0,native_triangles=0,native_group=0;
    bool drawn=false,refused=true;
    bool material_colours=false;
    std::array<float,3> native_ambient{},native_diffuse{};
    // Phong (model 2): engine workspace specular/power, also sent as the ABI3 part material.
    bool specular=false;std::array<float,3> native_specular{};float native_power=0;
    A2FORefitPartMaterial3 part_material{};
    // Specular map (Renderer CAP_SPECULAR_MAPS): uploaded loose texture masking the
    // highlight. Without a Phong material the configured default highlight is used.
    bool has_map=false;std::string map_name;A2FORefitPartMaterial3 map_material{};
};
struct MeshScope {
    std::array<GroupPlan,2> plans{};
    unsigned plan_count=0,draws=0;
    std::uint32_t route=0,triangle_budget=0;
    float raster_offset=0;
    std::uint64_t view_ticket=0;
    std::shared_ptr<const native11_world_lights::Snapshot> odf_lights;
    std::vector<A2FORefitLight> registered_lights;
    std::vector<A2FORefitSpot3> spots; // ABI3 ODF spotlights, shared by every plan of this mesh
    refit::Presentation2 source_presentation{};
    bool refused=true; // whole scope: nested/view boundaries void every plan
    bool material_colours=false; // any plan uses authored colours: recheck presentation at draw
    // [Refit]DiagnoseGroups observation of two-group legacy meshes. Never draws.
    bool diagnose=false;unsigned diag_id=0,diag_draws=0;
    std::array<std::uint32_t,2> diag_triangles{};
};
struct State {
    HMODULE image=nullptr,helper=nullptr;
    DWORD owner=0;Log log=nullptr;
    const A2FORefitApi2* api=nullptr;
    const A2FORefitApi3* api3=nullptr; // optional: same session, adds prepare_parts3 spotlights
    A2FORefitDisplayReplaceOpaque2 replace=nullptr;
    A2FORefitDisplayReplaceMesh2 replace_mesh=nullptr;
    A2FORefitDisplayLastCaller2 last_caller=nullptr;
    A2FORefitDisplayRaster2 raster=nullptr;
    A2FORefitDisplayReady2 ready=nullptr;
    std::unique_ptr<refit::Resources2> resources;
    std::array<Scope,16> scopes{};
    unsigned depth=0,overflow=0;
    std::array<MeshScope,16> meshes{};
    unsigned mesh_depth=0,mesh_overflow=0;
    std::uint32_t serial=0,epoch=1;
    bool attempted=false,blocked=false,submitted=false;
    bool render_scope_seen=false,posed_source_seen=false,prepared_seen=false;
    bool mesh_descriptor_seen=false;
    bool submit_meshes=false;
    bool use_odf_lights=false;
    bool use_material_colours=false;
    bool diagnose_colours=false;unsigned colour_diagnostics=0;
    bool extended_rhw=false;
    std::set<std::string> material_colour_draws;
    bool odf_light_diagnostic_seen=false;
    bool diagnose_groups=false;unsigned group_diagnostics=0;
    bool audit_upstream=false,upstream_audited=false;
    bool specular_maps=false,api3_maps=false;float map_strength=.5f,map_power=24.f;
    std::set<std::string> failed_maps;
    bool two_group_meshes=false,two_group_prepared=false;unsigned two_group_submitted_mask=0;
    unsigned parity_explanations=0;
    refit::ViewScopes2 views;
    bool mesh_seen[2]{};
    std::map<std::tuple<std::uint64_t,std::uint32_t,std::uint32_t>,std::shared_ptr<const GeometrySource>> geometry;
    std::size_t geometry_bytes=0;
    std::set<std::string> reasons;
    // Coverage telemetry: bounded counters, periodic summary (never changes behaviour).
    std::map<std::string,unsigned long long> tally;
    unsigned long long replaced_single=0,replaced_two=0;
    ULONGLONG last_summary=0;unsigned summaries=0;
};
State state; // Core and helpers are pinned for process lifetime.
template<class T>T proc(HMODULE module,const char* name) noexcept {
    const auto raw=GetProcAddress(module,name);T out=nullptr;
    static_assert(sizeof(out)==sizeof(raw));std::memcpy(&out,&raw,sizeof(out));return out;
}
void tally(const std::string& key) noexcept {
    try{if(state.tally.size()<256||state.tally.count(key))++state.tally[key];}catch(...){}
}
void coverage_summary() noexcept {
    try{
        if(!state.log||state.summaries>=120)return;
        const ULONGLONG now=GetTickCount64();
        if(state.last_summary&&now-state.last_summary<30000)return;
        if(!state.last_summary){state.last_summary=now;return;}
        state.last_summary=now;++state.summaries;
        std::vector<std::pair<unsigned long long,std::string>> top;
        for(const auto& [k,v]:state.tally)top.emplace_back(v,k);
        std::sort(top.begin(),top.end(),[](const auto& a,const auto& b){return a.first>b.first;});
        std::string line="[RefitScene] coverage #"+std::to_string(state.summaries)+" replaced single="+
            std::to_string(state.replaced_single)+" two_group="+std::to_string(state.replaced_two);
        for(std::size_t i=0;i<top.size()&&i<24;++i)line+=" | "+std::to_string(top[i].first)+"x "+top[i].second.substr(0,90);
        state.log(line.c_str());
    }catch(...){}
}
void report(const std::string& reason) noexcept {
    tally(reason);
    try{if(state.log&&state.reasons.size()<64&&state.reasons.insert(reason).second)
        state.log(("[RefitScene] "+reason).c_str());}catch(...){}
}
bool read(std::uintptr_t address,void* output,std::size_t n) noexcept {
    SIZE_T copied=0;
    return address&&n<=UINTPTR_MAX-address&&ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(address),output,n,&copied)&&copied==n;
}
bool profile_matches(HMODULE image) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    if(!native11_capture::exact_engine_image(image))return false;
    const auto matches=[&](const auto& regions) {
    for(const auto& region:regions){
        std::array<std::uint8_t,512> actual{};
        if(region.size>actual.size()||!read(base+region.rva,actual.data(),region.size))return false;
        const auto delta=static_cast<std::uint32_t>(base)-0x400000u;
        for(std::size_t i=0;i<region.relocation_count;++i){
            const auto offset=region.relocations[i];if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,actual.data()+offset,4);
            value-=delta;std::memcpy(actual.data()+offset,&value,4);
        }
        if(std::memcmp(actual.data(),region.bytes,region.size))return false;
    }
    return true;
    };
    if(!matches(profile::regions)||!matches(material_colours_profile::rhw_regions)||
       (state.use_material_colours&&!matches(material_colours_profile::regions)))return false;
    return true;
}
bool load_host() noexcept {
    try{
    if(state.ready&&state.ready()!=A2FO_REFIT_OK)return false;
    if(state.attempted){
        if(!state.api||!state.resources||state.blocked)return false;
        A2FORefitStatus status{};status.size=sizeof(status);
        const auto current=state.api->status(&status);
        if(current==A2FO_REFIT_OK)return true;
        if(current==A2FO_REFIT_STALE&&state.epoch!=UINT32_MAX){
            // Display retains the physical GPU across a facade Reset/window
            // handoff. The API explicitly retires old content before adopting
            // that new device epoch; never reuse old per-instance tokens.
            const auto next=state.epoch+1;std::string error;
            if(state.api->reset_content(next)==A2FO_REFIT_OK&&state.resources->reset_local(state.api,next,error)){
                for(auto& s:state.meshes){
                    for(auto& p:s.plans){p.packet.reset();p.refused=true;p.drawn=false;}
                    s.refused=true;
                }
                state.epoch=next;state.serial=0;
                report("scene content refreshed after Display device epoch change");return true;
            }
        }
        state.blocked=true;report("scene device/content state refused; further live replacement disabled");return false;
    }
        HMODULE display=nullptr;std::string path;unsigned backend=0;
        if(!refit::live_host_context(display,path,backend))return false;
        state.ready=proc<A2FORefitDisplayReady2>(display,"A2FORefitDisplay_RenderReady_v2");
        if(!state.ready){report("Display lacks render readiness query; live parts deferred");return false;}
        if(state.ready()!=A2FO_REFIT_OK)return false;
        state.attempted=true;
        state.replace=proc<A2FORefitDisplayReplaceOpaque2>(display,"A2FORefitDisplay_ReplaceOpaque_v2");
        state.replace_mesh=proc<A2FORefitDisplayReplaceMesh2>(display,"A2FORefitDisplay_ReplaceMesh_v2");
        const auto rhw=proc<A2FORefitDisplayReplaceMeshRhw3>(display,"A2FORefitDisplay_ReplaceMeshRhw_v3");
        if(rhw){state.replace_mesh=rhw;state.extended_rhw=true;}
        if(state.use_material_colours&&!rhw){report("Display lacks opt-in RHW callback; authored material colour replacement deferred");return false;}
        state.last_caller=proc<A2FORefitDisplayLastCaller2>(display,"A2FORefitDisplay_LastCaller_v2");
        state.raster=proc<A2FORefitDisplayRaster2>(display,"A2FORefitDisplay_Raster_v2");
        if(!state.replace){report("Display lacks scoped opaque replacement; original draws retained");return false;}
        if(!state.replace_mesh||!state.last_caller||!state.raster){report("Display lacks checked mesh/caller/raster scope API; original draws retained");return false;}
        state.helper=LoadLibraryExA(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!state.helper){report("scene helper load refused; original draws retained");return false;}
        auto get=proc<A2FORefitGetInterface>(state.helper,"A2FORefitRenderer_GetInterface");
        state.api=get?reinterpret_cast<const A2FORefitApi2*>(get(2)):nullptr;
        if(!state.api||state.api->size!=sizeof(A2FORefitApi2)||state.api->abi_version!=2||
           (state.api->capabilities&448)!=448){state.api=nullptr;report("ABI2 parts capabilities unavailable");return false;}
        // ABI3 is the ABI2 table plus spotlights. Without it, spots stay refused as before.
        if(const auto* v3=reinterpret_cast<const A2FORefitApi3*>(get(3));v3&&v3->size==sizeof(A2FORefitApi3)&&
           v3->abi_version==3&&(v3->capabilities&A2FO_REFIT_CAP_SPOTS)&&v3->prepare_parts==state.api->prepare_parts&&
           v3->draw_prepared==state.api->draw_prepared&&v3->prepare_parts3){state.api3=v3;report("Renderer ABI3 spotlights available");
            state.api3_maps=(v3->capabilities&A2FO_REFIT_CAP_SPECULAR_MAPS)!=0;
            if(state.specular_maps)report(state.api3_maps?"Renderer specular maps available":"SpecularMaps=1 but Renderer lacks specular maps; highlights unmasked");}
        HMODULE pinned=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(get),&pinned)||pinned!=state.helper){state.api=nullptr;return false;}
        A2FORefitConfig config{sizeof(config),display,0,state.epoch};
        if(state.api->initialize(&config)!=A2FO_REFIT_OK){state.api=nullptr;report("scene device admission refused; original draws retained");return false;}
        state.resources=std::make_unique<refit::Resources2>(state.api);
        report(backend==1?"Vulkan live parts host initialized":"DX12 live parts host initialized");
        return true;
    }catch(...){state.blocked=true;return false;}
}
// These fields are copied by the checked native main-view / GameObject render
// code before instance rendering. Never invoke game methods from this listener.
bool presentation(void* camera,refit::CameraView2& view,refit::Presentation2& out,
    std::vector<A2FORefitLight>& lights) {
    native11::EngineCameraView captured{};
    if(!native11_capture::camera_view(camera,captured))return false;
    std::memcpy(view.world_to_clip,captured.world_to_clip,sizeof(view.world_to_clip));
    std::memcpy(view.viewport,captured.viewport,sizeof(view.viewport));
    const auto base=reinterpret_cast<std::uintptr_t>(state.image);
    std::uint32_t storm=0;
    if(!read(base+0x3ad508,&storm,4)||!storm||
       !read(storm+0x28,out.ambient,sizeof(out.ambient))||
       !read(storm+0x34,out.team,sizeof(out.team))||
       !read(reinterpret_cast<std::uintptr_t>(camera)+0xe4,view.eye,sizeof(view.eye)))return false;
    std::uint8_t team_on=0;
    if(!read(storm+0x40,&team_on,1))return false;
    out.flags=team_on?A2FO_REFIT_TEAM:0;out.self_illum=1;
    std::uint32_t head=0,node=0;
    if(!read(storm+0x60,&head,4)||!head||!read(head,&node,4))return false;
    std::set<std::uint32_t> seen;
    unsigned directionals=0;
    while(node!=head){
        if(!node||seen.size()>=256||!seen.insert(node).second)return false;
        std::uint32_t record=0,light=0,vt=0,next=0;
        if(!read(node+8,&record,4)||!record||!read(record,&light,4)||!light||
           !read(light,&vt,4)||!read(node,&next,4))return false;
        A2FORefitLight l{};l.intensity=1;
        if(!read(record+4,l.colour,sizeof(l.colour)))return false;
        if(vt==base+0x2bc8fc){
            if(++directionals>4)return false;
            l.kind=A2FO_REFIT_DIRECTIONAL;
            if(!read(record+0x28,l.direction,sizeof(l.direction)))return false;
        }else if(vt==base+0x2bc8ac){
            l.kind=A2FO_REFIT_POINT;
            if(!read(record+0x34,l.position,sizeof(l.position))||
               !read(light+0x100,&l.start,4)||!read(light+0x104,&l.range,4))return false;
        }else return false; // unknown light shapes cannot be treated as points
        if(lights.size()>=32)return false;
        lights.push_back(l);node=next;
    }
    return true;
}
struct GeometryCopy {
    std::uint32_t node=0,group=0,triangles=0;
    native11::ModelGeometryPolicy policy{};
    bool found=false;
    std::shared_ptr<GeometrySource> output;
};
int copy_group_material(void* context,const A2FO_N11_GroupMaterial* material) noexcept {
    try {
        auto& output=*static_cast<GeometrySource*>(context);
        if(!material||!material->material_name||!material->material_name_length||
           material->material_name_length>255||!output.material_name.empty())return 0;
        output.material_name.assign(reinterpret_cast<const char*>(material->material_name),material->material_name_length);
        output.material=*material;output.material.material_name=nullptr;return 1;
    }catch(...){return 0;}
}
int copy_geometry(void* context,const A2FO_N11_Part* part,const A2FO_N11_PartSource* source) noexcept {
    auto& copy=*static_cast<GeometryCopy*>(context);
    try {
        if(!part||!source)return 0;
        if(source->node_index!=copy.node||source->group_index!=copy.group)return 1;
        if(copy.found||!part->vertices||!part->indices||!part->vertex_count||
           part->vertex_count>6144||part->index_count!=copy.triangles*3)return 0;
        std::vector<float> positions;
        positions.reserve(std::size_t(part->vertex_count)*3);
        for(std::uint32_t i=0;i<part->vertex_count;++i)
            positions.insert(positions.end(),part->vertices[i].position,part->vertices[i].position+3);
        std::string error;
        if(!native11::apply_model_geometry_policy(copy.policy,positions.data(),part->vertex_count,error))return 0;
        copy.output->triangles.reserve(copy.triangles);
        for(std::uint32_t i=0;i<copy.triangles;++i){
            refit::AuthoredTriangle2 triangle{};
            // The CPU Native11 adapter reverses corners 1/2. Refit's SOD scene
            // loader keeps authored winding, so undo only that conversion.
            constexpr unsigned corners[3]{0,2,1};
            for(unsigned j=0;j<3;++j){
                const auto index=part->indices[i*3+corners[j]];
                if(index>=part->vertex_count)return 0;
                const auto& vertex=part->vertices[index];const float* p=positions.data()+index*3;
                triangle[j]={p[0],p[1],p[2],vertex.uv[0],vertex.uv[1]};
            }
            copy.output->triangles.push_back(triangle);
        }
        copy.found=true;return 1;
    }catch(...){return 0;}
}
std::shared_ptr<const GeometrySource> geometry_source(const refit::MeshSelection2& selected) {
    const auto model=selected.observation->model;
    const auto key=std::make_tuple(model->info.serial,selected.node_index,selected.group_index);
    const auto found=state.geometry.find(key);
    if(found!=state.geometry.end())return found->second->model==model?found->second:nullptr;
    if(!selected.triangles||selected.triangles>2048||state.geometry.size()>=256)return nullptr;
    const auto bytes=std::size_t(selected.triangles)*sizeof(refit::AuthoredTriangle2);
    if(bytes>16u*1024u*1024u-state.geometry_bytes)return nullptr;
    GeometryCopy copy{selected.node_index,selected.group_index,selected.triangles,
        {model->info.parser_scalar_bits,model->info.parser_native_x87_control},false,
        std::make_shared<GeometrySource>()};
    copy.output->model=model;
    if(!a2fo_rs_native11_inspect_sod_all(model->bytes.data(),static_cast<std::uint32_t>(model->bytes.size()),
       &copy_geometry,&copy)||!copy.found)return nullptr;
    if(!a2fo_rs_native11_inspect_sod_group_material(model->bytes.data(),static_cast<std::uint32_t>(model->bytes.size()),
       selected.node_index,selected.group_index,&copy_group_material,copy.output.get()))return nullptr;
    state.geometry.emplace(key,copy.output);state.geometry_bytes+=bytes;return copy.output;
}
bool material_current(const GroupPlan& current) {
    if(!current.geometry)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(state.image);
    std::uint32_t storm=0,override_material=0,material=0,vt=0,name=0,model=0;
    std::uint8_t self_illum=0,team=0;
    float ambient[3]{},diffuse[3]{},specular[3]{},power=0;
    if(!read(base+0x3ad508,&storm,4)||!storm||!read(storm+0xa8,&override_material,4)||override_material||
       !read(current.native_group+0x14,&material,4)||!material||material>UINT32_MAX-0x46||
       !read(material,&vt,4)||vt!=base+0x2bc9a4||!read(material+8,&name,4)||!name||
       !read(material+0x18,ambient,sizeof(ambient))||!read(material+0x24,diffuse,sizeof(diffuse))||
       !read(material+0x30,specular,sizeof(specular))||!read(material+0x3c,&power,4)||
       !read(material+0x40,&model,4)||!read(material+0x44,&self_illum,1)||!read(material+0x45,&team,1))return false;
    const auto& source=current.geometry->material;
    if(model!=source.model||self_illum!=source.self_illuminating||power!=source.power||
       std::memcmp(ambient,source.ambient,sizeof(ambient))||std::memcmp(diffuse,source.diffuse,sizeof(diffuse))||
       std::memcmp(specular,source.specular,sizeof(specular)))return false;
    const auto& expected=current.geometry->material_name;
    if(expected.empty()||expected.size()>255||name>UINT32_MAX-expected.size())return false;
    bool team_name=expected.size()>=4;
    constexpr char prefix[]="team";
    for(unsigned i=0;i<4&&team_name;++i){const unsigned char c=expected[i];
        team_name=(c>='A'&&c<='Z'?c+('a'-'A'):c)==static_cast<unsigned char>(prefix[i]);}
    if(team!=static_cast<unsigned>(team_name))return false;
    std::array<char,256> actual{};
    if(!read(name,actual.data(),expected.size()+1)||actual[expected.size()]!=0||
       std::memcmp(actual.data(),expected.data(),expected.size()))return false;
    // The checked base selector binds mesh+0x114 slot0. Confirm it still names
    // the exact captured texture, rather than only pinning the old asset owner.
    if(!current.observation||current.observation->parts.size()!=1)return false;
    const auto& binding=current.observation->parts[0];
    std::uint32_t slots=0,count=0,texture=0;
    return binding.texture&&binding.native_node&&binding.native_node<=UINT32_MAX-0x120&&
        read(binding.native_node+0x114,&slots,4)&&slots&&
        read(binding.native_node+0x11c,&count,4)&&count>=1&&count<=8&&
        read(slots,&texture,4)&&texture==binding.texture->texture;
}
bool material_colours_current(const GroupPlan& current,std::uintptr_t workspace) noexcept {
    if(!current.material_colours)return true;
    float ambient[3]{},diffuse[3]{};
    if(workspace>UINT32_MAX-0x70||!read(workspace+0x58,ambient,sizeof(ambient))||
       !read(workspace+0x64,diffuse,sizeof(diffuse)))return false;
    for(unsigned i=0;i<3;++i)
        if(ambient[i]!=current.native_ambient[i]||diffuse[i]!=current.native_diffuse[i])return false;
    if(current.specular){
        float specular[3]{},power=0;
        if(!read(workspace+0x70,specular,sizeof(specular))||!read(workspace+0x7c,&power,4)||
           power!=current.native_power)return false;
        for(unsigned i=0;i<3;++i)if(specular[i]!=current.native_specular[i])return false;
    }
    return true;
}
void report_material_colour_draw(const GroupPlan& current) noexcept {
    try {
        if(!current.material_colours||!current.observation||!current.observation->model||
           !current.geometry||state.material_colour_draws.size()>=8)return;
        const std::string model=current.observation->model->info.name;
        if(state.material_colour_draws.insert(model).second)
            report("material colours drawn model="+model+" material="+current.geometry->material_name+
                " lighting_model="+std::to_string(current.geometry->material.model));
    }catch(...){} // Logging must never turn a successful draw into native fallback.
}
bool workspace_matches(const A2FORefitDraw2& draw,const GroupPlan& current,std::uintptr_t& output,refit::RhwLayout2& layout) noexcept {
    A2FORefitCaller2 caller{sizeof(caller),0,0};
    if(!state.last_caller||state.last_caller(&caller)!=A2FO_REFIT_OK||
       caller.sequence!=draw.sequence||!caller.caller_esi)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(state.image);
    const std::uintptr_t workspace=caller.caller_esi;
    std::uint32_t vt=0,vertices=0,vertices_end=0,indices=0,indices_end=0,format=0,stride=0,start=0;
    if(workspace>UINT32_MAX-0xc0||!read(workspace,&vt,4)||vt!=base+0x2bcd8c||
       !read(workspace+4,&vertices,4)||!read(workspace+0x30,&vertices_end,4)||
       !read(workspace+0x40,&indices,4)||!read(workspace+0x2c,&indices_end,4)||
       !read(workspace+0x98,&format,4)||!read(workspace+0x9c,&stride,4)||!read(workspace+0xbc,&start,4)||
       !vertices||!indices||vertices_end<vertices||indices_end<indices||format!=draw.vertex_format||!current.geometry)return false;
    std::string error;
    if(!refit::rhw_layout2(format,stride,current.geometry->material.model,layout,error))return false;
    const std::uint32_t vertex_bytes=vertices_end-vertices,index_bytes=indices_end-indices;
    if(vertex_bytes%stride||index_bytes%6||vertex_bytes/stride!=draw.args[2]||
       start!=draw.args[3]||index_bytes/6!=draw.args[4])return false;
    output=workspace;return true;
}
// [Refit]DiagnoseColours=1, observation only (max 8 lines): summarise the engine's
// own per-vertex lit colours (RHW D3DCOLOR at +16) for a draw that is about to be
// considered for replacement. The darkest vertex approximates the ambient term,
// the brightest the fully lit term. Compared with the presentation inputs this
// shows which native terms the modern colour model reproduces.
void diagnose_native_colours(const A2FORefitDraw2& draw,std::uintptr_t workspace,const MeshScope& scope,
    const GroupPlan& current,const refit::RhwLayout2& layout) noexcept {
    try {
        if(!state.diagnose_colours||state.colour_diagnostics>=8||!current.geometry||!draw.args[2]||draw.args[2]>65535)return;
        std::uint32_t vertices=0;if(!read(workspace+4,&vertices,4)||!vertices)return;
        std::vector<std::uint8_t> bytes(std::size_t(draw.args[2])*layout.stride);
        if(!read(vertices,bytes.data(),bytes.size()))return;
        unsigned lo[3]{255,255,255},hi[3]{0,0,0};double sum[3]{};
        for(std::size_t v=0;v<draw.args[2];++v){std::uint32_t c=0;std::memcpy(&c,bytes.data()+v*layout.stride+16,4);
            const unsigned rgb[3]{(c>>16)&255u,(c>>8)&255u,c&255u};
            for(unsigned i=0;i<3;++i){lo[i]=std::min(lo[i],rgb[i]);hi[i]=std::max(hi[i],rgb[i]);sum[i]+=rgb[i];}}
        const auto& sp=scope.source_presentation;const double n=draw.args[2];
        char text[600]{};
        std::snprintf(text,sizeof(text),"native colour oracle #%u model=%.48s material=%.40s lighting_model=%u fvf=%x verts=%u min=%u/%u/%u max=%u/%u/%u mean=%.1f/%.1f/%.1f scene_ambient=%.3f/%.3f/%.3f team=%.3f/%.3f/%.3f team_on=%u ws_ambient=%.3f/%.3f/%.3f ws_diffuse=%.3f/%.3f/%.3f lights=%zu",
            state.colour_diagnostics+1,current.observation?current.observation->model->info.name:"?",current.geometry->material_name.c_str(),
            current.geometry->material.model,layout.format,draw.args[2],lo[0],lo[1],lo[2],hi[0],hi[1],hi[2],sum[0]/n,sum[1]/n,sum[2]/n,
            sp.ambient[0],sp.ambient[1],sp.ambient[2],sp.team[0],sp.team[1],sp.team[2],(sp.flags&A2FO_REFIT_TEAM)?1u:0u,
            current.native_ambient[0],current.native_ambient[1],current.native_ambient[2],
            current.native_diffuse[0],current.native_diffuse[1],current.native_diffuse[2],scope.registered_lights.size());
        std::string all(text);
        for(std::size_t i=0;i<scope.registered_lights.size()&&i<4;++i){const auto& l=scope.registered_lights[i];char t[200]{};
            std::snprintf(t,sizeof(t)," | light%u kind=%u pos=%.1f/%.1f/%.1f dir=%.2f/%.2f/%.2f col=%.2f/%.2f/%.2f int=%.2f start=%.1f range=%.1f",
                static_cast<unsigned>(i),l.kind,l.position[0],l.position[1],l.position[2],l.direction[0],l.direction[1],l.direction[2],
                l.colour[0],l.colour[1],l.colour[2],l.intensity,l.start,l.range);all+=t;}
        ++state.colour_diagnostics;report(all);
    }catch(...){}
}
bool geometry_matches(const A2FORefitDraw2& draw,std::uintptr_t workspace,const MeshScope& scope,const GroupPlan& current,const refit::RhwLayout2& layout) {
    if(!current.geometry||current.geometry->triangles.size()!=current.triangles||
       current.native_triangles!=current.triangles||scope.triangle_budget!=UINT32_MAX||
       !draw.args[4]||draw.args[4]>current.triangles||draw.args[2]>current.triangles*3)return false;
    std::uint32_t budget=0,vertices=0,indices=0;
    if(!read(reinterpret_cast<std::uintptr_t>(state.image)+0x32c3f4,&budget,4)||budget!=UINT32_MAX||
       !read(workspace+4,&vertices,4)||!read(workspace+0x40,&indices,4))return false;
    std::vector<std::uint8_t> native_vertices(std::size_t(draw.args[2])*layout.stride);
    std::vector<std::uint16_t> native_indices(std::size_t(draw.args[4])*3);
    if(!read(vertices,native_vertices.data(),native_vertices.size())||
       !read(indices,native_indices.data(),native_indices.size()*sizeof(std::uint16_t)))return false;
    std::vector<refit::ProjectedTriangle2> native;std::string decode_error;
    if(!refit::decode_rhw_triangles2(native_vertices,native_indices,layout,scope.raster_offset,native,decode_error)){
        report("native RHW decode refused: "+decode_error);return false;
    }
    const auto& frame=current.packet->frame();const auto& part=frame.parts[0];
    std::array<float,12> world{};std::array<float,16> view{},projection{};
    std::memcpy(world.data(),part.world,sizeof(part.world));
    std::memcpy(view.data(),frame.view,sizeof(frame.view));std::memcpy(projection.data(),frame.projection,sizeof(frame.projection));
    const double determinant=double(world[0])*(double(world[4])*world[8]-double(world[5])*world[7])-
        double(world[1])*(double(world[3])*world[8]-double(world[5])*world[6])+
        double(world[2])*(double(world[3])*world[7]-double(world[4])*world[6]);
    const auto flags=current.observation->parts[0].source_flags;
    const auto cull=flags&1?refit::GeometryCull2::None:
        determinant<0?refit::GeometryCull2::CounterClockwise:refit::GeometryCull2::Clockwise;
    std::uint32_t visible=0;std::string error;
    if(!refit::check_geometry_parity2(current.geometry->triangles,world,view,projection,
        {float(frame.x),float(frame.y),float(frame.width),float(frame.height)},cull,native,visible,error)){
        report("mesh geometry parity refused: "+error);
        if(scope.plan_count==2&&state.parity_explanations<6){
            ++state.parity_explanations;
            report("two-group parity detail #"+std::to_string(state.parity_explanations)+" group="+
                std::to_string(&current-scope.plans.data())+" model="+std::string(current.observation->model->info.name)+
                " error="+error+" "+refit::explain_geometry_parity2(current.geometry->triangles,world,view,projection,
                {float(frame.x),float(frame.y),float(frame.width),float(frame.height)},cull,native));
        }
        return false;
    }
    return visible==draw.args[4];
}
bool odf_lights_current(const MeshScope& current,void* camera) {
    if(!state.use_odf_lights&&!current.material_colours)return true;
    const auto id=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(camera));
    const auto ticket=state.views.current(id);
    if(state.use_odf_lights&&(!ticket||ticket!=current.view_ticket||!current.odf_lights||
       current.odf_lights->camera!=id||current.odf_lights->frame_generation!=ticket||
       !native11_world_lights::current(*current.odf_lights)))return false;
    refit::CameraView2 view{};refit::Presentation2 shown{};
    std::vector<A2FORefitLight> registered;
    if(!presentation(camera,view,shown,registered)||registered.size()!=current.registered_lights.size()||
       shown.flags!=current.source_presentation.flags)return false;
    for(unsigned i=0;i<3;++i)if(shown.ambient[i]!=current.source_presentation.ambient[i]||
        shown.team[i]!=current.source_presentation.team[i])return false;
    for(std::size_t i=0;i<registered.size();++i){const auto& a=registered[i];const auto& b=current.registered_lights[i];
        if(a.kind!=b.kind||a.intensity!=b.intensity||a.start!=b.start||a.range!=b.range)return false;
        for(unsigned j=0;j<3;++j)if(a.position[j]!=b.position[j]||a.direction[j]!=b.direction[j]||a.colour[j]!=b.colour[j])return false;
    }
    if(!state.use_odf_lights)return true;
    // A fresh double-read roster is required at the actual draw, too. Merely
    // pinning a previous snapshot would not detect movement, creation or removal.
    std::shared_ptr<const native11_world_lights::Snapshot> now;std::string error;
    if(!native11_world_lights::sample(id,ticket,shown.ambient,now,error))return false;
    if(now)for(const auto& source:now->captured.sources)
        if(source.config.enabled&&source.config.legacy_native_source)return false;
    return now&&now->source_lifecycle==current.odf_lights->source_lifecycle&&
        now->class_generation==current.odf_lights->class_generation&&
        native11_world_lights::current(*now)&&
        refit::equal_world_lighting2(current.odf_lights->lighting,now->lighting);
}
// Spot-free frames keep the exact ABI2 call; spots require the ABI3 entry.
int32_t prepare_plan(const MeshScope& scope,const GroupPlan& plan) noexcept {
    if(scope.spots.empty()&&!plan.specular&&!plan.has_map)return state.api->prepare_parts(&plan.packet->frame());
    if(!state.api3||scope.spots.size()>A2FO_REFIT_MAX_SPOTS)return A2FO_REFIT_INVALID;
    const bool material=plan.specular||plan.has_map;
    const A2FORefitPartMaterial3 chosen=plan.specular?plan.part_material:plan.map_material;
    A2FORefitFrame3 frame{plan.packet->frame(),scope.spots.data(),static_cast<std::uint32_t>(scope.spots.size()),
        material?&chosen:nullptr,material?1u:0u};
    frame.base.size=sizeof(A2FORefitFrame3);
    if(!plan.has_map)return state.api3->prepare_parts3(&frame);
    if(!state.api3_maps)return A2FO_REFIT_INVALID;
    const A2FORefitPartMap3 map{plan.map_name.c_str(),static_cast<std::uint32_t>(plan.map_name.size()),1};
    A2FORefitFrame3Maps maps{frame,&map,1};
    maps.frame.base.size=sizeof(A2FORefitFrame3Maps);
    return state.api3->prepare_parts3(reinterpret_cast<const A2FORefitFrame3*>(&maps));
}
bool read_file(const std::string& path,std::vector<std::uint8_t>& out,std::size_t limit) noexcept {
    try{
        struct File{HANDLE h;~File(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}} f{CreateFileA(path.c_str(),GENERIC_READ,
            FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        LARGE_INTEGER size{};
        if(f.h==INVALID_HANDLE_VALUE||!GetFileSizeEx(f.h,&size)||size.QuadPart<=0||
           static_cast<unsigned long long>(size.QuadPart)>limit)return false;
        out.resize(static_cast<std::size_t>(size.QuadPart));DWORD got=0;
        return ReadFile(f.h,out.data(),static_cast<DWORD>(out.size()),&got,nullptr)&&got==out.size();
    }catch(...){return false;}
}
// The loose specular map A2FONebulaRenderer registered for this craft's diffuse
// (ART_CFG suffix, root precedence, faction fallback), read and uploaded once per path.
bool specular_map(const native11::MaterialObservation& observation,GroupPlan& plan) noexcept {
    try{
        plan.has_map=false;plan.map_name.clear();
        std::string diffuse;
        for(const auto& part:observation.parts){
            if(part.texture&&!part.texture->logical_name.empty()){diffuse=part.texture->logical_name;break;}
            if(!part.authored_texture.empty()){diffuse=part.authored_texture;break;}
        }
        std::string path;
        if(diffuse.empty()||!a2fo::nebula_specular_map_path(diffuse.c_str(),path)){tally("specular map: none registered");return false;}
        std::string key=path;for(auto& c:key)c=static_cast<char>(c>='A'&&c<='Z'?c+('a'-'A'):c);
        std::string name;
        if(!state.resources->cached(key,name)){
            if(state.failed_maps.count(key)){tally("specular map: previously refused");return false;}
            std::vector<std::uint8_t> bytes;std::uint32_t revision=0;std::string error;
            if(!read_file(path,bytes,32u*1024u*1024u)){state.failed_maps.insert(key);report("specular map unreadable or over 32 MB; highlight unmasked: "+path);return false;}
            if(!state.resources->encoded(key,bytes,name,revision,error)){
                state.failed_maps.insert(key);report("specular map refused ("+error+"); highlight unmasked: "+path);return false;
            }
            report("specular map uploaded for diffuse '"+diffuse+"': "+path);
        }
        plan.has_map=true;plan.map_name=std::move(name);
        const float s=state.map_strength;plan.map_material={{s,s,s},state.map_power};
        tally("specular map: applied");
        return true;
    }catch(...){plan.has_map=false;return false;}
}
int32_t mesh_opaque(void*,const A2FORefitDraw2* draw) noexcept {
    try {
        if(GetCurrentThreadId()!=state.owner||state.blocked||!state.submit_meshes||
           state.overflow||state.mesh_overflow||!state.depth||!state.mesh_depth||!draw)return 0;
        auto& scope=state.meshes[state.mesh_depth-1];
        if(scope.refused||!scope.plan_count)return 0;
        // Single-group scopes keep their original behaviour: every draw is
        // checked against plan 0 and a draw after a replacement is fatal.
        // Two-group scopes consume plans strictly by native draw order.
        const unsigned index=scope.plan_count==1?0u:scope.draws;
        ++scope.draws;
        const auto extra=[&]{
            state.blocked=true;scope.refused=true;
            report("unexpected extra opaque mesh submission; further replacements disabled; native extra draw retained");
            return 0;
        };
        if(index>=scope.plan_count){
            for(unsigned i=0;i<scope.plan_count;++i)if(scope.plans[i].drawn)return extra();
            return 0;
        }
        auto& current=scope.plans[index];
        if(current.refused||!current.packet||!current.observation)return 0;
        if(current.drawn)return extra();
        const auto& frame=current.packet->frame();
        if(!state.mesh_descriptor_seen){
            state.mesh_descriptor_seen=true;
            char detail[448]{};std::snprintf(detail,sizeof(detail),
                "first mesh draw descriptor: size=%u slot=%u caller=%08x fvf=%08x alpha=%u argc=%u args=%u/%u/%u/%u/%u triangles=%u native_triangles=%u initial_budget=%08x viewport=%u/%u/%u/%u expected=%u/%u/%u/%u",
                draw->size,draw->slot,draw->caller,draw->vertex_format,draw->alpha_test,draw->arg_count,
                draw->args[0],draw->args[1],draw->args[2],draw->args[3],draw->args[4],current.triangles,
                current.native_triangles,scope.triangle_budget,
                draw->viewport[0],draw->viewport[1],draw->viewport[2],draw->viewport[3],
                frame.x,frame.y,frame.width,frame.height);report(detail);
        }
        if(draw->size!=sizeof(*draw)||draw->slot!=71||draw->arg_count!=5||
           draw->caller!=reinterpret_cast<std::uintptr_t>(state.image)+0x248c01||
           ((draw->vertex_format!=0x1c4)&&(draw->vertex_format!=0x144||!state.extended_rhw))||draw->alpha_test||draw->args[0]!=4||draw->args[1]!=0||
           !draw->args[2]||draw->args[2]>65535||!draw->args[4]||draw->args[4]>current.triangles||
           draw->viewport[0]!=frame.x||draw->viewport[1]!=frame.y||
           draw->viewport[2]!=frame.width||draw->viewport[3]!=frame.height)return 0;
        std::uintptr_t workspace=0;refit::RhwLayout2 layout{};
        if(!workspace_matches(*draw,current,workspace,layout)){report("mesh draw does not match its native workspace; original retained");return 0;}
        if(!material_current(current)){report("mesh lighting material changed or overridden; original retained");return 0;}
        diagnose_native_colours(*draw,workspace,scope,current,layout);
        if(!material_colours_current(current,workspace)){report("native workspace material coefficients changed; original retained");return 0;}
        if(!odf_lights_current(scope,state.scopes[state.depth-1].camera)){
            report("lighting view/source changed before draw; original retained");return 0;
        }
        if(!geometry_matches(*draw,workspace,scope,current,layout))return 0;
        if(!refit_mesh_capture::draw_source_current()||
           native11_assets::pin_model(current.observation->database,current.observation->model_serial)!=current.observation->model)return 0;
        for(const auto& part:current.observation->parts)
            if(part.texture&&native11_textures::current(part.texture->texture)!=part.texture)return 0;
        const auto ready=prepare_plan(scope,current);
        if(ready!=A2FO_REFIT_OK){current.refused=true;report("mesh preflight refused code="+std::to_string(ready));return 0;}
        const auto result=state.api->draw_prepared(frame.source_epoch,frame.serial);
        if(result!=A2FO_REFIT_OK){
            current.refused=true;state.blocked=true;
            report("mesh draw failed code="+std::to_string(result)+"; further replacement disabled; frame may be partial");return 0;
        }
        current.drawn=true;
        if(scope.plan_count==2)++state.replaced_two;else ++state.replaced_single;
        report_material_colour_draw(current);
        if(scope.plan_count==1&&!state.submitted){state.submitted=true;report("first owned single-group mesh submitted through native scene API; gameplay/visual acceptance still required");}
        if(scope.plan_count==2&&!(state.two_group_submitted_mask&(1u<<index))){state.two_group_submitted_mask|=1u<<index;
            report("first owned two-group mesh group "+std::to_string(index)+" submitted through native scene API; gameplay/visual acceptance still required");}
        return 1;
    }catch(...){state.blocked=true;return 0;}
}
// Observation-only: every draw returns 0 so Display keeps the original native
// submission. Records how many checked opaque draws a two-group legacy mesh
// issues and which native group each one's triangle count matches.
// Observation-only twin of the original ABI2 workspace check (0x1c4, stride
// 32). The admission path uses workspace_matches with the group's layout.
bool diagnostic_workspace_matches(const A2FORefitDraw2& draw,std::uintptr_t& output) noexcept {
    A2FORefitCaller2 caller{sizeof(caller),0,0};
    if(!state.last_caller||state.last_caller(&caller)!=A2FO_REFIT_OK||
       caller.sequence!=draw.sequence||!caller.caller_esi)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(state.image);
    const std::uintptr_t workspace=caller.caller_esi;
    std::uint32_t vt=0,vertices=0,vertices_end=0,indices=0,indices_end=0,format=0,stride=0,start=0;
    if(workspace>UINT32_MAX-0xc0||!read(workspace,&vt,4)||vt!=base+0x2bcd8c||
       !read(workspace+4,&vertices,4)||!read(workspace+0x30,&vertices_end,4)||
       !read(workspace+0x40,&indices,4)||!read(workspace+0x2c,&indices_end,4)||
       !read(workspace+0x98,&format,4)||!read(workspace+0x9c,&stride,4)||!read(workspace+0xbc,&start,4)||
       !vertices||!indices||vertices_end<vertices||indices_end<indices||format!=0x1c4||stride!=32)return false;
    const std::uint32_t vertex_bytes=vertices_end-vertices,index_bytes=indices_end-indices;
    if(vertex_bytes%stride||index_bytes%6||vertex_bytes/stride!=draw.args[2]||
       start!=draw.args[3]||index_bytes/6!=draw.args[4])return false;
    output=workspace;return true;
}
int32_t mesh_diagnose(void*,const A2FORefitDraw2* draw) noexcept {
    try {
        if(GetCurrentThreadId()!=state.owner||!state.mesh_depth||!draw)return 0;
        auto& current=state.meshes[state.mesh_depth-1];
        if(!current.diagnose)return 0;
        const auto n=++current.diag_draws;
        if(n>4)return 0;
        std::uintptr_t workspace=0;
        const bool layout=draw->size==sizeof(*draw)&&draw->arg_count==5;
        const bool caller=draw->caller==reinterpret_cast<std::uintptr_t>(state.image)+0x248c01;
        const bool ws=layout&&diagnostic_workspace_matches(*draw,workspace);
        const auto tris=layout?draw->args[4]:0;
        char detail[384]{};
        std::snprintf(detail,sizeof(detail),
            "two-group diag %u draw %u: slot=%u caller_checked=%u fvf=%08x alpha=%u args=%u/%u/%u/%u/%u workspace=%u matches_group0=%u matches_group1=%u; native retained",
            current.diag_id,n,draw->slot,caller?1u:0u,draw->vertex_format,draw->alpha_test,
            layout?draw->args[0]:0,layout?draw->args[1]:0,layout?draw->args[2]:0,layout?draw->args[3]:0,tris,
            ws?1u:0u,tris&&tris==current.diag_triangles[0]?1u:0u,tris&&tris==current.diag_triangles[1]?1u:0u);
        report(detail);
    }catch(...){}
    return 0;
}
void diagnose_two_group(MeshScope& next,std::uintptr_t address,
    const std::shared_ptr<const native11::MaterialObservation>& observed) {
    if(state.group_diagnostics>=8)return;
    // Legacy body 0x631fd0 and dispatcher 0x632799 walk node+0x104 as inline
    // 0x1c-byte group records (count node+0x100); per-group draw 0x645cf0
    // reads triangles at +0xc. Material pointer +0x14 as in material_current.
    std::uint32_t groups_at=0;
    if(!read(address+0x104,&groups_at,4)||!groups_at||groups_at>UINT32_MAX-0x38)return;
    std::string groups_text;
    for(unsigned i=0;i<2;++i){
        const std::uint32_t g=groups_at+i*0x1c;std::uint32_t tris=0,material=0,vt=0,name=0;std::uint8_t team=0;
        if(!read(g+0xc,&tris,4)||!read(g+0x14,&material,4))return;
        next.diag_triangles[i]=tris;
        std::array<char,48> text{};
        if(material&&material<=UINT32_MAX-0x46&&read(material,&vt,4)&&read(material+8,&name,4)&&name){
            read(material+0x45,&team,1);
            for(std::size_t j=0;j+1<text.size();++j){char c=0;if(!read(name+j,&c,1)||!c)break;text[j]=(c>=32&&c<127)?c:'?';}
        }
        char part[160]{};
        std::snprintf(part,sizeof(part)," g%u{tris=%u lighting_vt=%s team=%u name=%s}",i,tris,
            vt==reinterpret_cast<std::uintptr_t>(state.image)+0x2bc9a4?"checked":"other",team,text.data());
        groups_text+=part;
    }
    std::string bindings;
    for(const auto& part:observed->parts)if(part.native_node==address){
        char b[96]{};std::snprintf(b,sizeof(b)," b{group=%u tris=%u flags=%u}",part.group_index,part.source_triangles,part.source_flags);
        bindings+=b;
    }
    if(!load_host())return;
    next.diagnose=true;next.diag_id=++state.group_diagnostics;
    char head[256]{};
    std::snprintf(head,sizeof(head),"two-group diag %u: model=%.96s node=%08x groups_at=%08x",
        next.diag_id,observed->model->info.name,static_cast<unsigned>(address),groups_at);
    report(std::string(head)+groups_text+bindings);
    if(state.replace_mesh(&mesh_diagnose,nullptr)!=A2FO_REFIT_OK){next.diagnose=false;report("two-group diag registration refused");}
}
void clear_callback() noexcept {if(state.replace)state.replace(nullptr,nullptr);}
// [Refit]AuditUpstreamSites=1: once a match is rendering (FleetOpsHook and every A2FO
// module installed), compare upstream render3d hook sites in process memory with their
// stock bytes. Read-only; reports which upstream modules' sites are still unpatched.
void audit_upstream_sites() noexcept {
    try{
        if(!state.audit_upstream||state.upstream_audited||!state.log)return;
        state.upstream_audited=true;
        const auto base=reinterpret_cast<std::uintptr_t>(state.image);
        struct Tally{unsigned stock=0,total=0;std::string changed;};
        std::map<std::string,Tally> modules;
        for(std::size_t i=0;i<refit_upstream_sites::site_count;++i){
            const auto& site=refit_upstream_sites::sites[i];auto& t=modules[site.module];++t.total;
            std::uint8_t actual[32]{};
            const bool readable=read(base+(site.va-0x400000u),actual,site.size);
            if(readable&&std::memcmp(actual,site.bytes,site.size)==0){++t.stock;continue;}
            if(t.changed.size()<200){char b[96]{};std::snprintf(b,sizeof(b)," %s@%08x%s",site.id,site.va,readable?"":"(unreadable)");t.changed+=b;}
        }
        unsigned stock=0,total=0;
        for(const auto& [name,t]:modules){stock+=t.stock;total+=t.total;
            state.log(("[RefitScene] upstream site audit "+name+": "+std::to_string(t.stock)+"/"+std::to_string(t.total)+
                " stock"+(t.changed.empty()?"":"; changed:"+t.changed)).c_str());}
        state.log(("[RefitScene] upstream site audit total: "+std::to_string(stock)+"/"+std::to_string(total)+
            " stock in process memory (read-only; no upstream hook installed)").c_str());
    }catch(...){}
}
void begin(void*,void* instance,void* camera) noexcept {
    if(GetCurrentThreadId()!=state.owner)return;
    audit_upstream_sites();
    if(!state.render_scope_seen){state.render_scope_seen=true;report("first checked instance render scope observed");}
    clear_callback();
    // A nested object can mutate shared model-database pose and consume newer
    // serials. Preserve the parent native path after that boundary.
    for(unsigned i=0;i<state.mesh_depth;++i)state.meshes[i].refused=true;
    if(state.depth)for(unsigned i=0;i<state.depth;++i)state.scopes[i].observation.reset();
    if(state.overflow||state.depth==state.scopes.size()){++state.overflow;return;}
    auto& next=state.scopes[state.depth++];next=Scope{};next.instance=instance;next.camera=camera;
}
void end(void*) noexcept {
    if(GetCurrentThreadId()!=state.owner)return;
    clear_callback();
    if(state.overflow){--state.overflow;return;}
    if(state.depth)state.scopes[--state.depth]=Scope{};
}
void parts(void*,void* camera,const native11::Object& object,
    const std::shared_ptr<const native11::MaterialObservation>& observed) noexcept {
    if(GetCurrentThreadId()!=state.owner||state.blocked||state.overflow||!state.depth)return;
    try{
        auto& scope=state.scopes[state.depth-1];
        if(scope.instance!=reinterpret_cast<void*>(object.instance)||scope.camera!=camera||scope.observation||!observed)return;
        if(!state.posed_source_seen){state.posed_source_seen=true;report("first authoritative posed material source observed");}
        if(native11_assets::pin_model(object.database,object.model_serial)!=observed->model)return;
        for(const auto& part:observed->parts)if(part.texture&&native11_textures::current(part.texture->texture)!=part.texture)return;
        scope.observation=observed;scope.object=object;
    }catch(...){report("live source allocation failure; original instance retained");}
}
bool native_base_material(void* material) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(state.image);
    std::uint32_t vt=0;
    // draw_source_current qualifies this exact vtable and all relevant native
    // methods. Matching Storm's generic default class would wrongly reject FO's
    // ordinary self-illuminating hull materials. Other classes stay native.
    return read(reinterpret_cast<std::uintptr_t>(material),&vt,4)&&vt==base+0x2bc854;
}
void mesh_scope(bool entering,void* node,std::uint32_t pass,void* material,
    std::uint32_t route,bool qualified) {
    if(GetCurrentThreadId()!=state.owner)return;
    clear_callback();
    if(!entering){
        if(state.mesh_overflow){--state.mesh_overflow;return;}
        if(!state.mesh_depth)return;
        if(const auto& done=state.meshes[state.mesh_depth-1];done.diagnose)
            report("two-group diag "+std::to_string(done.diag_id)+" complete: checked opaque draws="+std::to_string(done.diag_draws));
        state.meshes[--state.mesh_depth]=MeshScope{};
        // Never reuse a parent token after a nested mesh, even if the child
        // eventually retained its native draw.
        if(state.mesh_depth)state.meshes[state.mesh_depth-1].refused=true;
        return;
    }
    if(state.mesh_overflow||state.mesh_depth==state.meshes.size()){++state.mesh_overflow;return;}
    auto& next=state.meshes[state.mesh_depth++];next=MeshScope{};next.route=route;
    coverage_summary();
    if(state.submit_meshes&&qualified&&state.depth){
        if(route!=1)tally("scope: MeshVB route (native only)");
        else if(pass!=0)tally("scope: legacy pass "+std::to_string(pass)+" (native only)");
        else tally("scope: legacy pass 0 candidate");
    }
    if(!state.submit_meshes||state.blocked||!qualified||route!=1||pass!=0||
       state.overflow||!state.depth||!refit_mesh_capture::draw_source_current())return;
    const auto& instance=state.scopes[state.depth-1];
    const auto observed=instance.observation;
    if(!observed||!observed->model){tally("prep: no posed source");return;}
    if(!refit::mesh_model_format_supported2(*observed->model)){tally("prep: SOD version unsupported");return;}
    if(native11_assets::pin_model(observed->database,observed->model_serial)!=observed->model){tally("prep: model owner changed");return;}
    const auto address=reinterpret_cast<std::uintptr_t>(node);
    std::uint32_t groups=0;
    if(!address||address>UINT32_MAX-0x104||!read(address+0x100,&groups,4))return;
    if(groups!=1&&!(groups==2&&state.two_group_meshes))tally("prep: native groups="+std::to_string(groups<9?groups:9)+" not admitted");
    const bool two_group=groups==2&&state.two_group_meshes;
    if(groups==2&&!two_group&&state.diagnose_groups){diagnose_two_group(next,address,observed);return;}
    const unsigned count=two_group?2u:1u;
    std::array<refit::MeshSelection2,2> selections{};std::string error;
    for(unsigned g=0;g<count;++g){
        const bool ok=two_group?
            refit::select_opaque_group2(*observed,static_cast<std::uint32_t>(address),groups,g,selections[g],error):
            refit::select_opaque_mesh2(*observed,static_cast<std::uint32_t>(address),groups,selections[g],error);
        if(!ok){tally("select: "+error);return;}
    }
    // Native group records are inline 0x1c-byte entries at node+0x104
    // (legacy body 0x631fd0 / dispatcher 0x632799); +0xc is their triangle count.
    std::uint32_t groups_at=0;
    if(!read(address+0x104,&groups_at,4)||!groups_at||groups_at>UINT32_MAX-0x10-0x1c*(count-1)||
       !read(reinterpret_cast<std::uintptr_t>(state.image)+0x32c3f4,&next.triangle_budget,4)||
       next.triangle_budget!=UINT32_MAX)return;
    for(unsigned g=0;g<count;++g){
        auto& plan=next.plans[g];plan.native_group=groups_at+0x1c*g;
        if(!read(plan.native_group+0xc,&plan.native_triangles,4)||plan.native_triangles!=selections[g].triangles)return;
    }
    if(!native_base_material(material)){report("mesh texture-material class is outside the qualified base-pass profile; original retained");return;}
    for(unsigned g=0;g<count;++g)for(const auto& part:selections[g].observation->parts)
        if(part.texture&&native11_textures::current(part.texture->texture)!=part.texture)return;
    if(!load_host())return;
    for(unsigned g=0;g<count;++g){
        auto& plan=next.plans[g];
        plan.geometry=geometry_source(selections[g]);if(!plan.geometry)return;
        plan.observation=selections[g].observation;
        if(!material_current(plan)){report("mesh lighting material source refused; original retained");return;}
    }
    refit::CameraView2 camera{};refit::Presentation2 shown{};std::vector<A2FORefitLight> lights;
    if(!presentation(instance.camera,camera,shown,lights)){report("mesh camera/lighting source refused; original retained");return;}
    if(state.use_odf_lights||state.use_material_colours){
        next.registered_lights=lights;next.source_presentation=shown;
    }
    if(state.use_odf_lights){
        const auto id=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(instance.camera));
        next.view_ticket=state.views.current(id);if(!next.view_ticket)return;
        if(!native11_world_lights::sample(id,next.view_ticket,shown.ambient,next.odf_lights,error)){
            report("fresh ODF lighting source refused: "+error);return;
        }
        if(next.odf_lights)for(const auto& source:next.odf_lights->captured.sources)
            if(source.config.enabled&&source.config.legacy_native_source){
                report("ODF source also creates a native light; use a2foLightSource with lightSource=0 to avoid duplicate lighting");return;
            }
        std::vector<A2FORefitLight> combined;
        const bool converted=next.odf_lights&&(state.api3?
            refit::append_world_lights3(lights,next.odf_lights->lighting,combined,next.spots,error):
            refit::append_world_lights2(lights,next.odf_lights->lighting,combined,error));
        if(!converted){
            if(!state.odf_light_diagnostic_seen){state.odf_light_diagnostic_seen=true;
                for(std::size_t i=0;i<lights.size()&&i<8;++i){const auto& l=lights[i];char detail[384]{};
                    std::snprintf(detail,sizeof(detail),"registered light %u: kind=%u position=%g/%g/%g direction=%g/%g/%g colour=%g/%g/%g intensity=%g start=%g range=%g",
                        static_cast<unsigned>(i),l.kind,l.position[0],l.position[1],l.position[2],l.direction[0],l.direction[1],l.direction[2],
                        l.colour[0],l.colour[1],l.colour[2],l.intensity,l.start,l.range);report(detail);
                }
            }
            report("ODF light conversion refused: "+error);return;
        }
        lights=std::move(combined);
        if(!next.odf_lights->lighting.lights.empty())report("fresh ODF directional/point colour lighting included in scoped packet");
        if(!next.spots.empty())report("fresh ODF spotlights included through Renderer ABI3");
        if(next.odf_lights->lighting.shadows.light_mask)report("ODF shadow requests recorded; native Refit geometry shadows remain unavailable");
    }
    A2FORefitRaster2 raster{sizeof(raster),0};
    if(state.raster(&raster)!=A2FO_REFIT_OK||!std::isfinite(raster.rhw_to_scene_offset)||
       (raster.rhw_to_scene_offset!=0&&raster.rhw_to_scene_offset!=0.5f-1.f/128.f))return;
    next.raster_offset=raster.rhw_to_scene_offset;
    for(unsigned col=0;col<4;++col){
        camera.world_to_clip[col]+=2*next.raster_offset/camera.viewport[2]*camera.world_to_clip[12+col];
        camera.world_to_clip[4+col]-=2*next.raster_offset/camera.viewport[3]*camera.world_to_clip[12+col];
    }
    // The checked native class runs its own emissive pass after this lit base.
    // Keep that draw native rather than lighting the whole texture twice.
    shown.self_illum=0;
    const auto control=refit_scene_test::material_control();
    if(control){
        lights.clear();next.spots.clear();
        shown.flags=control==1?A2FO_REFIT_UNLIT:0;
        for(auto& value:shown.ambient)value=0;
        report(control==1?"isolated white material pixel control active":"isolated black material pixel control active");
    }
    refit::ModelUpload2 model{};
    if(!state.resources->model(selections[0].observation->model,model,error)){report("mesh model upload: "+error);return;}
    for(unsigned g=0;g<count;++g){
        auto& plan=next.plans[g];
        auto packet_observation=selections[g].observation;
        if(control){
            // Positive/negative pixel controls exist only inside the separately
            // admitted private scene fixture. Keep original observations for all
            // material/texture ownership checks and modify only the copied packet.
            auto diagnostic=std::make_shared<native11::MaterialObservation>(*packet_observation);
            for(auto& part:diagnostic->parts){part.authored_texture.clear();part.texture.reset();}
            packet_observation=std::move(diagnostic);
        }
        // Authored colours are per group: a base and its team_ group carry
        // different lighting materials. Shared presentation/lights stay intact.
        auto group_shown=shown;auto group_lights=lights;
        if(state.use_material_colours&&!control&&!next.spots.empty()){
            // The colour model pre-multiplies light colours by the group's diffuse;
            // spots are not folded yet, so keep this mesh native rather than mis-light it.
            report("authored material colours with ODF spotlights not yet supported; original retained");return;
        }
        if(state.use_material_colours&&!control){
            refit::MaterialColours2 material{};material.model=plan.geometry->material.model;
            const auto& name=plan.geometry->material_name;
            material.team=name.size()>=4;
            constexpr char prefix[]="team";
            for(unsigned i=0;i<4&&material.team;++i){const unsigned char c=name[i];
                material.team=(c>='A'&&c<='Z'?c+('a'-'A'):c)==static_cast<unsigned char>(prefix[i]);}
            std::copy_n(plan.geometry->material.ambient,3,material.ambient.begin());
            std::copy_n(plan.geometry->material.diffuse,3,material.diffuse.begin());
            std::copy_n(plan.geometry->material.specular,3,material.specular.begin());
            material.power=plan.geometry->material.power;
            if(material.model==2&&!state.api3){report("Phong material needs Renderer ABI3; original retained");return;}
            refit::PreparedMaterialColours2 prepared;
            if(!refit::prepare_material_colours2(material,shown,lights,prepared,error)){
                report("authored material colours refused: "+error+"; original retained");return;
            }
            plan.material_colours=true;plan.native_ambient=prepared.native_ambient;
            plan.native_diffuse=prepared.native_diffuse;group_shown=prepared.presentation;group_lights=std::move(prepared.lights);
            plan.specular=prepared.specular;plan.native_specular=prepared.native_specular;plan.native_power=prepared.native_power;
            if(plan.specular){std::copy_n(prepared.native_specular.begin(),3,plan.part_material.specular);plan.part_material.power=prepared.native_power;}
            next.material_colours=true;
            report(plan.specular?"authored Phong material colours and per-pixel specular included; native emissive retained":
                "authored Constant/Lambert material colours included; native emissive retained");
        }
        if(state.specular_maps&&state.api3_maps&&!control)specular_map(*selections[g].observation,plan);
        if(state.serial==UINT32_MAX){state.blocked=true;return;}
        if(!refit::build_packet2(instance.object,*packet_observation,camera,group_shown,model.id,model.revision,model.node_count,
            state.epoch,++state.serial,group_lights,state.resources.get(),refit::Resources2::resolve_texture,plan.packet,error)){
            report("mesh packet refused: "+error);return;
        }
        if(plan.packet->frame().part_count!=1){plan.packet.reset();return;}
        const auto ready=prepare_plan(next,plan);
        if(ready!=A2FO_REFIT_OK){plan.packet.reset();report("mesh parts preflight refused code="+std::to_string(ready));return;}
        plan.triangles=selections[g].triangles;
    }
    // Every group prepared: arm all plans together, or none.
    for(unsigned g=0;g<count;++g)next.plans[g].refused=false;
    next.plan_count=count;next.refused=false;
    if(state.replace_mesh(&mesh_opaque,nullptr)!=A2FO_REFIT_OK){next.refused=true;report("mesh scope registration refused");return;}
    if(count==1&&!state.prepared_seen){state.prepared_seen=true;report("first owned single-group packet prepared at pre-batch mesh boundary");}
    if(count==2&&!state.two_group_prepared){state.two_group_prepared=true;report("first owned two-group packets prepared at pre-batch mesh boundary (one per native group draw)");}
}
void mesh(void* node,std::uint32_t pass,void* material,std::uint32_t route) {
    if(GetCurrentThreadId()!=state.owner||route>1||state.overflow||!state.depth||state.mesh_seen[route])return;
    const auto& scope=state.scopes[state.depth-1];
    const auto source=scope.observation;
    if(!source||native11_assets::pin_model(source->database,source->model_serial)!=source->model)return;
    const auto address=reinterpret_cast<std::uintptr_t>(node);
    unsigned matching=0;std::uint32_t groups=0;
    if(!address||address>UINT32_MAX-0x104||!read(address+0x100,&groups,4)||!groups||groups>8192)return;
    for(const auto& part:source->parts)if(part.native_node==address){
        if(part.group_index>=groups)return;
        ++matching;
    }
    if(!matching)return;
    char message[320]{};
    std::snprintf(message,sizeof(message),"first owned pre-batch mesh: route=%s model=%.128s node=%08x groups=%u owned_parts=%u pass=%u material=%08x; observation only, native submission retained",
        route?"legacy":"MeshVB",source->model->info.name,static_cast<unsigned>(address),groups,
        matching,pass,static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(material)));
    state.mesh_seen[route]=true;report(message);
}
}
bool initialize(HMODULE image,const std::string& root,Log log) noexcept {
    if(state.owner)return false;
    state.log=log;
    try{
        bool enabled=false;
        const auto roots=a2fo::discover_extension_roots(root,GetCommandLineA());
        for(const auto& r:roots.roots){char value[16]{};
            GetPrivateProfileStringA("Refit","LiveParts",enabled?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("LiveParts must be 0 or 1");return false;}
            enabled=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","SubmitMeshes",state.submit_meshes?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("SubmitMeshes must be 0 or 1");return false;}
            state.submit_meshes=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","OdfLights",state.use_odf_lights?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("OdfLights must be 0 or 1");return false;}
            state.use_odf_lights=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","DiagnoseGroups",state.diagnose_groups?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("DiagnoseGroups must be 0 or 1");return false;}
            state.diagnose_groups=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","TwoGroupMeshes",state.two_group_meshes?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("TwoGroupMeshes must be 0 or 1");return false;}
            state.two_group_meshes=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","AuditUpstreamSites",state.audit_upstream?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("AuditUpstreamSites must be 0 or 1");return false;}
            state.audit_upstream=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","MaterialColours",state.use_material_colours?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("MaterialColours must be 0 or 1");return false;}
            state.use_material_colours=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","DiagnoseColours",state.diagnose_colours?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("DiagnoseColours must be 0 or 1");return false;}
            state.diagnose_colours=std::strcmp(value,"1")==0;
            GetPrivateProfileStringA("Refit","SpecularMaps",state.specular_maps?"1":"0",value,sizeof(value),(r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){report("SpecularMaps must be 0 or 1");return false;}
            state.specular_maps=std::strcmp(value,"1")==0;
            const auto number=[&](const char* key,float& out,float lo,float hi){
                char text[32]{};GetPrivateProfileStringA("Refit",key,"",text,sizeof(text),(r+"\\A2FORenderer.ini").c_str());
                if(!*text)return true;char* end=nullptr;const float v=std::strtof(text,&end);
                if(!end||*end||!(v>=lo&&v<=hi)){report(std::string(key)+" out of range");return false;}
                out=v;return true;};
            if(!number("SpecularMapStrength",state.map_strength,0.f,4.f)||!number("SpecularMapPower",state.map_power,1.f,1024.f))return false;
        }
        if(!enabled){report("LiveParts=0; native Display passthrough retained");return false;}
        if(!profile_matches(image)){report("live lighting/source profile refused");return false;}
        state.image=image;state.owner=GetCurrentThreadId();
        if(!native11_materials::set_listener({&begin,&parts,&end,nullptr})){
            state.blocked=true;report("render listener ownership refused");return false;
        }
        if(!refit_mesh_capture::initialize(image,&mesh,log,&mesh_scope))
            report("pre-batch mesh observer unavailable; original native submissions retained");
        report(state.submit_meshes?"SubmitMeshes=1; only checked single-group standard meshes eligible":
            "SubmitMeshes=0; pre-batch mesh observation only");
        if(state.submit_meshes&&state.two_group_meshes)report("TwoGroupMeshes=1; checked two-group (base + team) legacy meshes eligible, one packet per native group draw");
        if(state.diagnose_groups)report("DiagnoseGroups=1; two-group legacy meshes observed only (max 8), native draws retained");
        if(state.use_odf_lights)report("OdfLights=1; fresh per-view directional/point colour sources required; CaptureWorldLights producer must be enabled");
        if(state.specular_maps)report("SpecularMaps=1; registered _specular maps mask the per-pixel highlight (Phong material, else SpecularMapStrength/SpecularMapPower)");
        if(state.use_material_colours)report("MaterialColours=1; authored Constant/Lambert/Phong colours (Phong: per-pixel specular via Renderer ABI3) and native workspace/source rechecks required");
        report("live per-instance sources armed; helper loading deferred until posed draw");return true;
    }catch(...){return false;}
}
std::uint64_t begin_view(void* camera,bool capture_owned) noexcept {
    if(!state.use_odf_lights||!state.owner||GetCurrentThreadId()!=state.owner)return 0;
    clear_callback();
    for(unsigned i=0;i<state.mesh_depth;++i)state.meshes[i].refused=true;
    return state.views.enter(reinterpret_cast<std::uintptr_t>(camera),capture_owned);
}
void end_view(void* camera,std::uint64_t ticket) noexcept {
    if(!ticket||!state.owner||GetCurrentThreadId()!=state.owner)return;
    clear_callback();
    for(unsigned i=0;i<state.mesh_depth;++i)state.meshes[i].refused=true;
    state.views.leave(reinterpret_cast<std::uintptr_t>(camera),ticket);
    if(state.views.faulted())report("ODF lighting view scope mismatch; ODF replacement disabled until restart");
}
}
