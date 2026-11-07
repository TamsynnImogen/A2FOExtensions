#include "../renderer/native11/presentation_session.hpp"
#include <windows.h>
#include <cassert>
#include <memory>
#include <string>
#include <thread>
using namespace a2fo::native11;
namespace {
PresentationSession* active=nullptr;
int creates=0,destroys=0,submits=0,presents=0,configures=0,resizes=0;
int submit_result=S_OK,present_result=S_OK;bool reentry_refused=false;
std::uint64_t last_serial=0;
std::int32_t A2FO_N11_CALL create(const A2FO_N11_Host*,A2FO_N11_Session* id){++creates;*id=17;return S_OK;}
std::int32_t A2FO_N11_CALL destroy(A2FO_N11_Session){++destroys;return S_OK;}
std::int32_t A2FO_N11_CALL configure(A2FO_N11_Session,const A2FO_N11_PresentationTarget* t){++configures;assert(t->struct_size==sizeof(*t));return S_OK;}
std::int32_t A2FO_N11_CALL resize(A2FO_N11_Session,std::uint32_t,std::uint32_t){++resizes;return S_OK;}
std::int32_t A2FO_N11_CALL submit(A2FO_N11_Session,const A2FO_N11_PrimitiveFrame* frame){
    ++submits;assert(frame->generation>last_serial);last_serial=frame->generation;
    std::string error;reentry_refused=!active->close(error);return submit_result;
}
std::int32_t A2FO_N11_CALL present(A2FO_N11_Session,const A2FO_N11_Present* request){
    ++presents;assert(request->struct_size==sizeof(*request));return present_result;
}
bool current(void* raw,const FrameIdentity&) noexcept {auto* count=static_cast<int*>(raw);++*count;return *count!=4;}
bool always(void*,const FrameIdentity&) noexcept {return true;}
bool live(void* raw,const FrameIdentity&) noexcept {
    assert(!active->current());return *static_cast<bool*>(raw);
}
bool revoked_after_present(void* raw,const FrameIdentity&) noexcept {
    auto* count=static_cast<int*>(raw);return ++*count!=5;
}
A2FO_N11_API8 api(){
    A2FO_N11_API8 a{};auto& b=a.base.base.base.base.base.base.base;
    b.struct_size=sizeof(a);b.version=8;b.capabilities=511;b.create=create;b.destroy=destroy;b.resize=resize;
    b.submit=reinterpret_cast<decltype(b.submit)>(+[](A2FO_N11_Session,const A2FO_N11_Frame*)->std::int32_t{return S_OK;});
    b.readback=reinterpret_cast<decltype(b.readback)>(+[](A2FO_N11_Session,A2FO_N11_Readback*)->std::int32_t{return S_OK;});
    b.usage=reinterpret_cast<decltype(b.usage)>(+[](A2FO_N11_Session,A2FO_N11_Usage*)->std::int32_t{return S_OK;});
    a.base.base.base.base.base.base.configure_shadows=+[](A2FO_N11_Session,const A2FO_N11_Shadows*)->std::int32_t{return S_OK;};
    a.base.base.base.base.base.base.configure_materials=+[](A2FO_N11_Session,const A2FO_N11_Materials*)->std::int32_t{return S_OK;};
    a.base.base.base.base.base.configure_texture_bindings=+[](A2FO_N11_Session,const A2FO_N11_TextureBindings*)->std::int32_t{return S_OK;};
    a.base.base.base.base.configure_part_states=+[](A2FO_N11_Session,const A2FO_N11_PartStates*)->std::int32_t{return S_OK;};
    a.base.base.base.submit_world=+[](A2FO_N11_Session,const A2FO_N11_Frame*)->std::int32_t{return S_OK;};
    a.base.base.configure_mesh_sharing=+[](A2FO_N11_Session,const A2FO_N11_MeshSharing*)->std::int32_t{return S_OK;};
    a.base.submit_primitives=submit;a.configure_presentation=configure;a.present=present;return a;
}
std::shared_ptr<const FrameBundle> bundle(std::uint64_t generation,bool complete=true){
    auto p=std::make_shared<PrimitiveFrame>();p->generation=generation;p->source_epoch=1;
    p->settings.viewport[2]=8;p->settings.viewport[3]=8;
    PrimitiveMesh mesh{};mesh.id=1;mesh.vertices.resize(3);mesh.indices={0,1,2};p->meshes.push_back(mesh);
    PrimitiveDraw draw{};draw.mesh=1;p->draws.push_back(draw);
    auto b=std::make_shared<FrameBundle>();b->identity={1,generation,1,1};b->width=b->height=8;
    b->complete_geometry=complete;b->absent_domains=all_frame_domains&~1u;
    b->passes.push_back({FrameDomain::background,p});return b;
}
}
int main(){
    PresentationSession session;active=&session;std::string error;auto table=api();
    assert(session.initialize(table,8,8,reinterpret_cast<HWND>(1),error));
    assert(session.render(bundle(1,false),always,nullptr,error)==PresentationResult::fallback&&submits==0&&presents==0);
    int checks=0;auto revoked=std::make_shared<FrameBundle>(*bundle(1));
    auto second=std::make_shared<PrimitiveFrame>(*revoked->passes[0].frame);
    second->settings.clear_colour_target=false;revoked->passes.push_back({FrameDomain::background,second});
    assert(session.render(revoked,current,&checks,error)==PresentationResult::fallback&&submits==1&&presents==0);
    assert(session.render(bundle(1),always,nullptr,error)==PresentationResult::presented);
    assert(reentry_refused&&session.current()&&session.accepted_generation()==1&&presents==1);
    assert(session.render(bundle(1),always,nullptr,error)==PresentationResult::fallback&&!session.current());
    int after_present=0;assert(session.render(bundle(2),revoked_after_present,&after_present,error)==PresentationResult::fallback);
    assert(after_present==5&&!session.current()&&presents==2);
    present_result=S_FALSE;after_present=0;
    assert(session.render(bundle(2),revoked_after_present,&after_present,error)==PresentationResult::fallback&&after_present==5);
    assert(session.render(bundle(2),always,nullptr,error)==PresentationResult::occluded&&!session.current());
    present_result=S_OK;submit_result=E_FAIL;
    assert(session.render(bundle(2),always,nullptr,error)==PresentationResult::fallback&&presents==4&&!session.current());
    submit_result=S_OK;assert(session.resize(16,16,reinterpret_cast<HWND>(1),error));
    assert(session.render(bundle(3),always,nullptr,error)==PresentationResult::fallback);
    assert(session.resize(0,0,nullptr,error));
    assert(session.render(bundle(3),always,nullptr,error)==PresentationResult::fallback);
    assert(session.resize(8,8,reinterpret_cast<HWND>(1),error));
    bool wrong_thread=false;std::thread other([&]{std::string e;wrong_thread=!session.close(e);});other.join();assert(wrong_thread);
    assert(session.render(bundle(3),always,nullptr,error)==PresentationResult::presented);
    assert(!session.resize(0,8,reinterpret_cast<HWND>(1),error)&&!session.current());
    assert(session.render(bundle(4),always,nullptr,error)==PresentationResult::fallback);
    assert(session.resize(8,8,reinterpret_cast<HWND>(1),error));
    bool source_valid=true;assert(session.render(bundle(4),live,&source_valid,error)==PresentationResult::presented);
    assert(session.current()&&session.accepted_generation()==4);
    source_valid=false;assert(!session.current()&&session.accepted_generation()==0);
    assert(session.close(error)&&destroys==1);last_serial=0;
    assert(session.initialize(table,8,8,reinterpret_cast<HWND>(1),error));
    assert(session.render(bundle(1),always,nullptr,error)==PresentationResult::presented);
    assert(session.close(error)&&creates==2&&destroys==2);
}
