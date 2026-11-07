// No game hooks or exception handlers: the optional helper waits out of process.
#include "../../sdk/include/a2fo_module_api.h"
#include "config.hpp"
#include "../../rust/module_runtime_a2_ffi.h"
#include <vector>
#include <exception>

namespace {
HANDLE cancel_event = nullptr;
bool started = false;
std::exception_ptr callback_error;
struct MailerContext {
    const A2FO_ModuleApi* api = nullptr;
    std::wstring root;
    std::wstring helper;
    log_mailer::Config config;
    HANDLE parent = nullptr;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    std::vector<unsigned char> attributes;
    LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
    bool initialized = false;
    bool success = false;
};
void log(const A2FO_ModuleApi* api, const char* message) {
    if (api->log) api->log("A2FOLogMailer", message);
}
std::uint8_t mailer_step(void* opaque,std::uint32_t op) noexcept {
    if (callback_error) return 0;
    auto& state=*static_cast<MailerContext*>(opaque);
    try {
        switch(op) {
        case 0: {
            const char* root_text=state.api->root_directory();
            if (!root_text || !*root_text) return 0;
            const int length=MultiByteToWideChar(CP_ACP,0,root_text,-1,nullptr,0);
            if (!length) return 0;
            std::vector<wchar_t> root_buffer(length);
            MultiByteToWideChar(CP_ACP,0,root_text,-1,root_buffer.data(),length);
            state.root=root_buffer.data();
            state.config=log_mailer::load_config(state.root);
            return 1;
        }
        case 1: return state.config.enabled;
        case 2: return state.config.ready();
        case 3:
            state.helper=log_mailer::join(state.root,L"modules\\A2FOLogMailerHelper.exe");
            return DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),
                &state.parent,SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,TRUE,0)!=FALSE;
        case 4:
            cancel_event=CreateEventW(&state.security,TRUE,FALSE,nullptr);
            return cancel_event!=nullptr;
        case 5: {
            SIZE_T bytes=0;
            InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
            state.attributes.resize(bytes);
            state.list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(state.attributes.data());
            state.initialized=InitializeProcThreadAttributeList(state.list,1,0,&bytes)!=FALSE;
            return state.initialized;
        }
        case 6: {
            HANDLE handles[]{state.parent,cancel_event};
            return UpdateProcThreadAttribute(state.list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                handles,sizeof(handles),nullptr,nullptr)!=FALSE;
        }
        case 7: {
            std::wstring command=log_mailer::quote(state.helper)+L" --watch "+
                std::to_wstring(reinterpret_cast<std::uintptr_t>(state.parent))+L" "+
                std::to_wstring(reinterpret_cast<std::uintptr_t>(cancel_event))+L" "+
                log_mailer::quote(state.root);
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb=sizeof(startup);
            startup.lpAttributeList=state.list;
            PROCESS_INFORMATION child{};
            state.success=CreateProcessW(state.helper.c_str(),command.data(),nullptr,nullptr,
                TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,
                state.root.c_str(),&startup.StartupInfo,&child)!=FALSE;
            if(state.success){CloseHandle(child.hThread);CloseHandle(child.hProcess);}
            return state.success;
        }
        case 8: CloseHandle(state.parent);return 1;
        case 9:
            if(state.initialized)DeleteProcThreadAttributeList(state.list);
            CloseHandle(state.parent);
            if(!state.success){CloseHandle(cancel_event);cancel_event=nullptr;}
            return 1;
        case 10: started=true;return 1;
        case 11: return state.config.all_sessions;
        case 12:
            if(cancel_event){SetEvent(cancel_event);CloseHandle(cancel_event);}
            cancel_event=nullptr;started=false;return 1;
        default:return 0;
        }
    } catch (...) { callback_error=std::current_exception();return 0; }
}
void mailer_log(void* opaque,std::uint32_t code) noexcept {
    if(callback_error)return;
    auto& state=*static_cast<MailerContext*>(opaque);
    try {
        log(state.api,code==0?"Disabled: no helper, log collection or network activity.":
            code==1?"Inactive: configure an HTTPS RelayUrl and SubmissionToken first.":
            code==2?"Helper could not start; crash reporting is inactive.":
            code==3?"Opt-in all-session log emails enabled; recipient aclaydon1@googlemail.com.":
                    "Opt-in crash-only log emails enabled; recipient aclaydon1@googlemail.com.");
    } catch (...) {callback_error=std::current_exception();}
}
}

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(
    const A2FO_ModuleApi* api) {
    const bool valid=api && api->api_version==A2FO_MODULE_API_VERSION &&
        A2FO_MODULE_API_HAS(api,root_directory) && api->root_directory;
    if(!valid)return false;
    MailerContext context;context.api=api;
    callback_error=nullptr;
    const bool result=a2fo_rs_runtime_mailer_init(valid,started,&mailer_step,&mailer_log,&context)!=0;
    if(callback_error)std::rethrow_exception(callback_error);
    return result;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    MailerContext context;
    callback_error=nullptr;
    a2fo_rs_runtime_mailer_shutdown(&mailer_step,&context);
    if(callback_error)std::rethrow_exception(callback_error);
}
