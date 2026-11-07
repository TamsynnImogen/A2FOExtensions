#include "native_startup.hpp"
#include <cwchar>
#include <cstring>
namespace a2fo::native11 {
namespace {
void fail(std::string& error,const char* message) noexcept {try{error=message;}catch(...){error.clear();}}
bool absolute(const std::wstring& path) noexcept {
    if(path.empty()||path.find(L'\0')!=std::wstring::npos)return false;
    // Reject relative, drive-relative and rooted-on-current-drive paths.
    const bool drive=path.size()>=3&&((path[0]>=L'A'&&path[0]<=L'Z')||(path[0]>=L'a'&&path[0]<=L'z'))&&
        path[1]==L':'&&(path[2]==L'\\'||path[2]==L'/');
    const bool unc=path.size()>=3&&path[0]==L'\\'&&path[1]==L'\\'&&path[2]!=L'\\';
    return drive||unc;
}
}
bool NativeStartup::initialize(const std::wstring& path,std::uint32_t w,std::uint32_t h,HWND window,std::string& error) noexcept {
    if(dll_||!absolute(path)){fail(error,"native11 DLL path is not absolute or startup is active");return false;}
    dll_=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!dll_){fail(error,"native11 DLL load failed");return false;}
    const auto symbol=GetProcAddress(dll_,"A2FO_Native11_GetInterface");
    if(!symbol){fail(error,"native11 API export missing");std::string cleanup;shutdown(cleanup);return false;}
    A2FO_N11_GetInterface get=nullptr;
    static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API8 api{};
    if(get(A2FO_N11_API_VERSION_PRESENTATION,sizeof(api),&api)!=S_OK){
        fail(error,"native11 API8 unavailable");std::string cleanup;shutdown(cleanup);return false;
    }
    if(!session_.initialize(api,w,h,window,error)){
        std::string cleanup;shutdown(cleanup);
        return false;
    }
    initialized_=true;error.clear();return true;
}
PresentationResult NativeStartup::render(std::shared_ptr<const FrameBundle> packet,SourceCurrent current,void* context,std::string& error) noexcept {
    if(!initialized_){fail(error,"native11 startup inactive");return PresentationResult::fallback;}
    return session_.render(std::move(packet),current,context,error);
}
bool NativeStartup::resize(std::uint32_t w,std::uint32_t h,HWND window,std::string& error) noexcept {
    if(!initialized_){fail(error,"native11 startup inactive");return false;}
    return session_.resize(w,h,window,error);
}
bool NativeStartup::shutdown(std::string& error) noexcept {
    if(!dll_){error.clear();return true;}
    if(!session_.close(error))return false;
    initialized_=false;
    if(!FreeLibrary(dll_)){fail(error,"native11 DLL unload failed");return false;}
    dll_=nullptr;initialized_=false;error.clear();return true;
}
std::shared_ptr<const FrameBundle> NativeStartup::current() const noexcept {return initialized_?session_.current():nullptr;}
std::uint64_t NativeStartup::accepted_generation() const noexcept {return initialized_?session_.accepted_generation():0;}
}
