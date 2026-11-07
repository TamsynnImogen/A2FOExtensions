#include "presentation_session.hpp"
#include <cstdio>
#include <limits>
#include <vector>

namespace a2fo::native11 {
namespace {
void fail(std::string& error,const char* message) noexcept {try{error=message;}catch(...){error.clear();}}
void failed(std::string& error,const char* operation,std::int32_t hr) noexcept {
    char message[112]{};std::snprintf(message,sizeof(message),"native11 presentation %s failed (HRESULT %08lx)",operation,
        static_cast<unsigned long>(static_cast<std::uint32_t>(hr)));fail(error,message);
}
struct Busy { bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;} };
const A2FO_N11_API& base(const A2FO_N11_API8& a) noexcept {return a.base.base.base.base.base.base.base;}
bool valid(const A2FO_N11_API8& a) noexcept {
    const auto& h=base(a);
    return sizeof(a)==76&&h.struct_size==sizeof(a)&&h.version==A2FO_N11_API_VERSION_PRESENTATION&&
        !h.reserved&&(h.capabilities&511u)==511u&&h.create&&h.destroy&&h.submit&&h.resize&&h.readback&&h.usage&&
        a.base.base.base.base.base.base.configure_shadows&&a.base.base.base.base.base.base.configure_materials&&
        a.base.base.base.base.base.configure_texture_bindings&&a.base.base.base.base.configure_part_states&&
        a.base.base.base.submit_world&&a.base.base.configure_mesh_sharing&&a.base.submit_primitives&&
        a.configure_presentation&&a.present;
}
bool dimensions(std::uint32_t w,std::uint32_t h) noexcept {
    return w&&h&&w<=16384&&h<=16384&&std::uint64_t(w)*h*8<=256ull*1024*1024;
}
A2FO_N11_PresentationTarget target(std::uint32_t w,std::uint32_t h,HWND window) noexcept {
    A2FO_N11_PresentationTarget t{};t.struct_size=sizeof(t);t.window=window;t.width=w;t.height=h;return t;
}
}
bool PresentationSession::available() const noexcept {return session_&&thread_==GetCurrentThreadId()&&!busy_;}
std::int32_t A2FO_N11_CALL PresentationSession::unused_asset(void*,std::uint32_t,const std::uint8_t*,std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*) noexcept {return 0;}
bool PresentationSession::initialize(const A2FO_N11_API8& api,std::uint32_t w,std::uint32_t h,HWND window,std::string& error) noexcept {
    if(session_||busy_||!valid(api)||!dimensions(w,h)||!window){fail(error,"invalid or active presentation session");return false;}
    Busy guard(busy_);api_=api;thread_=GetCurrentThreadId();
    A2FO_N11_Host host{sizeof(host),0,w,h,this,unused_asset,nullptr};
    const auto hr=base(api_).create(&host,&session_);
    if(hr!=S_OK||!session_){if(!session_){api_={};thread_=0;}failed(error,"create",hr);return false;}
    width_=w;height_=h;auto t=target(w,h,window);
    const auto attach=api_.configure_presentation(session_,&t);
    if(attach!=S_OK){failed(error,"attach",attach);return false;}
    attached_=true;error.clear();return true;
}
PresentationResult PresentationSession::render(std::shared_ptr<const FrameBundle> packet,SourceCurrent source_current,void* context,std::string& error) noexcept {
    if(available()){current_.reset();source_current_=nullptr;source_context_=nullptr;}
    if(!available()||!attached_||!packet||!source_current||packet->identity.generation<=accepted_||
       packet->width!=width_||packet->height!=height_){fail(error,"presentation unavailable, reentrant, wrong target or stale frame");return PresentationResult::fallback;}
    Busy guard(busy_);
    if(!validate_frame_bundle(*packet,error))return PresentationResult::fallback;
    try {
        // Encode the complete candidate before touching any GPU state.
        std::vector<EncodedPrimitiveFrame> wires;wires.reserve(packet->passes.size());
        for(const auto& pass:packet->passes){
            EncodedPrimitiveFrame wire;
            if(!encode_primitive_frame(pass.frame,wire,error))return PresentationResult::fallback;
            wires.push_back(std::move(wire));
        }
        if(!source_current(context,packet->identity)){fail(error,"presentation source revoked");return PresentationResult::fallback;}
        for(auto& wire:wires){
            if(!source_current(context,packet->identity)){fail(error,"presentation source revoked");return PresentationResult::fallback;}
            if(serial_==std::numeric_limits<std::uint64_t>::max()){fail(error,"presentation serial exhausted");return PresentationResult::fallback;}
            wire.frame.generation=++serial_;
            const auto hr=api_.base.submit_primitives(session_,&wire.frame);
            if(hr!=S_OK){failed(error,"submit",hr);return PresentationResult::fallback;}
            if(!source_current(context,packet->identity)){fail(error,"presentation source revoked");return PresentationResult::fallback;}
        }
        if(!source_current(context,packet->identity)){fail(error,"presentation source revoked");return PresentationResult::fallback;}
        A2FO_N11_Present request{};request.struct_size=sizeof(request);
        const auto hr=api_.present(session_,&request);
        const bool still_current=source_current(context,packet->identity);
        if(!still_current){fail(error,"presentation source revoked");return PresentationResult::fallback;}
        if(hr==S_FALSE){error.clear();return PresentationResult::occluded;}
        if(hr!=S_OK){failed(error,"present",hr);return PresentationResult::fallback;}
        accepted_=packet->identity.generation;current_=std::move(packet);
        source_current_=source_current;source_context_=context;error.clear();return PresentationResult::presented;
    }catch(...){fail(error,"presentation packet allocation failed");return PresentationResult::fallback;}
}
bool PresentationSession::resize(std::uint32_t w,std::uint32_t h,HWND window,std::string& error) noexcept {
    if(!available()){fail(error,"presentation resize unavailable or reentrant");return false;}
    Busy guard(busy_);current_.reset();source_current_=nullptr;source_context_=nullptr;attached_=false;
    const bool detach=!w&&!h&&!window;
    if(!detach&&(!dimensions(w,h)||!window)){fail(error,"invalid presentation target");return false;}
    if(detach){auto t=target(0,0,nullptr);const auto hr=api_.configure_presentation(session_,&t);
        if(hr!=S_OK){failed(error,"detach",hr);return false;}width_=height_=0;error.clear();return true;}
    const auto resized=base(api_).resize(session_,w,h);
    if(resized!=S_OK){failed(error,"resize",resized);return false;}
    width_=w;height_=h;auto t=target(w,h,window);
    const auto hr=api_.configure_presentation(session_,&t);
    if(hr!=S_OK){failed(error,"attach",hr);return false;}
    attached_=true;error.clear();return true;
}
bool PresentationSession::close(std::string& error) noexcept {
    if((thread_&&thread_!=GetCurrentThreadId())||busy_){fail(error,"presentation close unavailable or reentrant");return false;}
    if(!session_){error.clear();return true;}
    Busy guard(busy_);current_.reset();source_current_=nullptr;source_context_=nullptr;attached_=false;
    auto t=target(0,0,nullptr);const auto detached=api_.configure_presentation(session_,&t);
    if(detached!=S_OK){failed(error,"detach",detached);return false;}
    const auto hr=base(api_).destroy(session_);
    if(hr!=S_OK){failed(error,"destroy",hr);return false;}
    session_=0;thread_=0;width_=height_=0;serial_=accepted_=0;api_={};error.clear();return true;
}
std::shared_ptr<const FrameBundle> PresentationSession::current() const noexcept {
    if(!available()||!current_||!source_current_)return nullptr;
    Busy guard(busy_);
    if(!source_current_(source_context_,current_->identity)){
        current_.reset();source_current_=nullptr;source_context_=nullptr;return nullptr;
    }
    return current_;
}
std::uint64_t PresentationSession::accepted_generation() const noexcept {return current()?accepted_:0;}
}
