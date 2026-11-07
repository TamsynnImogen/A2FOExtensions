#include "primitive_session.hpp"
#include <windows.h>
#include <cstdio>
#include <limits>
#include <utility>
namespace a2fo::native11 {
namespace {
void fail(std::string& out,const char* text) noexcept {try{out=text;}catch(...){out.clear();}}
void failed(std::string& out,const char* operation,std::int32_t hr,const std::string& detail) noexcept {
    char text[112]{};std::snprintf(text,sizeof(text),"native11 primitive %s failed (HRESULT %08lx)",operation,
        static_cast<unsigned long>(static_cast<std::uint32_t>(hr)));fail(out,text);
    if(!detail.empty())try{out+=": ";out+=detail;}catch(...){}
}
struct Busy {bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
const A2FO_N11_API& base(const A2FO_N11_API7& api) noexcept {return api.base.base.base.base.base.base;}
bool valid(const A2FO_N11_API7& api) noexcept {
    const auto& h=base(api);
    return h.struct_size==sizeof(api)&&h.version==A2FO_N11_API_VERSION_PRIMITIVES&&!h.reserved&&
        (h.capabilities&255u)==255u&&h.create&&h.destroy&&h.submit&&h.resize&&h.readback&&h.usage&&
        api.base.base.base.base.base.configure_shadows&&api.base.base.base.base.base.configure_materials&&
        api.base.base.base.base.configure_texture_bindings&&api.base.base.base.configure_part_states&&
        api.base.base.submit_world&&api.base.configure_mesh_sharing&&api.submit_primitives;
}
bool dimensions(std::uint32_t w,std::uint32_t h) noexcept {return w&&h&&w<=16384&&h<=16384&&std::uint64_t(w)*h*8<=256ull*1024*1024;}
}
bool PrimitiveRenderSession::available() const noexcept {return thread_==GetCurrentThreadId()&&session_&&!busy_;}
bool PrimitiveRenderSession::initialize(const A2FO_N11_API7& api,std::uint32_t w,std::uint32_t h,std::string& error) noexcept {
    if(session_||busy_||!valid(api)||!dimensions(w,h)){fail(error,"invalid or active native11 primitive session");return false;}
    Busy guard(busy_);api_=api;thread_=GetCurrentThreadId();last_log_.clear();
    A2FO_N11_Host host{sizeof(host),0,w,h,this,unused_asset,log};
    const auto result=base(api_).create(&host,&session_);
    if(result<0||!session_){session_=0;thread_=0;failed(error,"create",result,last_log_);return false;}
    width_=w;height_=h;image_eligible_=false;generation_=0;current_.reset();error.clear();return true;
}
bool PrimitiveRenderSession::render(std::shared_ptr<const PrimitiveFrame> packet,std::string& error) noexcept {
    if(!available()||!packet||!packet->generation||packet->generation<=generation_){fail(error,"unavailable, reentrant or stale primitive frame");return false;}
    Busy guard(busy_);EncodedPrimitiveFrame wire;
    if(!encode_primitive_frame(packet,wire,error))return false;
    last_log_.clear();const auto hr=api_.submit_primitives(session_,&wire.frame);
    if(hr<0){failed(error,"submit",hr,last_log_);return false;}
    current_=std::move(packet);generation_=current_->generation;image_eligible_=true;error.clear();return true;
}
bool PrimitiveRenderSession::resize(std::uint32_t w,std::uint32_t h,std::string& error) noexcept {
    if(!available()||!dimensions(w,h)){fail(error,"invalid primitive resize request");return false;}
    Busy guard(busy_);if(w!=width_||h!=height_)image_eligible_=false;last_log_.clear();
    const auto hr=base(api_).resize(session_,w,h);
    if(hr<0){image_eligible_=false;failed(error,"resize",hr,last_log_);return false;}
    width_=w;height_=h;error.clear();return true;
}
bool PrimitiveRenderSession::readback(std::vector<std::uint8_t>& output,std::uint32_t& w,std::uint32_t& h,std::string& error) noexcept {
    if(!available()||!image_eligible_){fail(error,"no current initialized primitive image");return false;}
    Busy guard(busy_);last_log_.clear();
    try {
        A2FO_N11_Readback read{};read.struct_size=sizeof(read);auto hr=base(api_).readback(session_,&read);
        const auto metadata=[&](){return read.struct_size==sizeof(read)&&read.width==width_&&read.height==height_&&
            read.pitch==std::uint64_t(width_)*4&&read.required_bytes==std::uint64_t(width_)*height_*4&&
            read.required_bytes<=std::numeric_limits<std::size_t>::max();};
        if(hr<0){failed(error,"readback metadata",hr,last_log_);return false;}
        if(hr!=S_FALSE||!metadata()||read.bgra||read.capacity){fail(error,"invalid primitive readback metadata response");return false;}
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(read.required_bytes));
        read.bgra=pixels.data();read.capacity=pixels.size();hr=base(api_).readback(session_,&read);
        if(hr<0){failed(error,"readback pixels",hr,last_log_);return false;}
        if(hr!=S_OK||!metadata()||read.bgra!=pixels.data()||read.capacity!=pixels.size()){
            fail(error,"primitive pixel response changed metadata or did not return pixels");return false;}
        output=std::move(pixels);w=width_;h=height_;error.clear();return true;
    }catch(...){fail(error,"primitive readback allocation failure");return false;}
}
bool PrimitiveRenderSession::usage(A2FO_N11_Usage& output,std::string& error) noexcept {
    if(!available()){fail(error,"primitive usage unavailable or reentrant");return false;}
    Busy guard(busy_);A2FO_N11_Usage next{};next.struct_size=sizeof(next);last_log_.clear();
    const auto hr=base(api_).usage(session_,&next);if(hr<0){failed(error,"usage",hr,last_log_);return false;}
    if(hr!=S_OK||next.struct_size!=sizeof(next)||next.reserved){fail(error,"invalid primitive usage response");return false;}
    output=next;error.clear();return true;
}
bool PrimitiveRenderSession::close(std::string& error) noexcept {
    if((thread_&&thread_!=GetCurrentThreadId())||busy_){fail(error,"primitive close unavailable or reentrant");return false;}
    if(!session_){error.clear();return true;}
    Busy guard(busy_);last_log_.clear();const auto hr=base(api_).destroy(session_);
    if(hr<0){failed(error,"destroy",hr,last_log_);return false;}
    session_=0;thread_=width_=height_=0;current_.reset();generation_=0;image_eligible_=false;api_={};error.clear();return true;
}
std::shared_ptr<const PrimitiveFrame> PrimitiveRenderSession::current() const noexcept {return available()?current_:nullptr;}
std::uint64_t PrimitiveRenderSession::accepted_generation() const noexcept {return available()?generation_:0;}
std::int32_t A2FO_N11_CALL PrimitiveRenderSession::unused_asset(void*,std::uint32_t,const std::uint8_t*,std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*) noexcept {return 0;}
void A2FO_N11_CALL PrimitiveRenderSession::log(void* raw,const char* text,std::uint32_t count) noexcept {
    if(!raw||!text||!count)return;
    auto& self=*static_cast<PrimitiveRenderSession*>(raw);
    if(self.thread_!=GetCurrentThreadId()||!self.busy_)return;
    try{self.last_log_.assign(text,text+(count<256?count:256));}catch(...){self.last_log_.clear();}
}
}
