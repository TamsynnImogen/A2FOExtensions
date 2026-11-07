#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "../../sdk/include/a2fo_module_api.h"
#include "recovery_tests.hpp"
#include "signatures.hpp"
#include "../../rust/module_runtime_b_ffi.h"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

static_assert(sizeof(void*)==4,"Recovery module requires Windows i386");
extern "C" {
void a2fo_recovery_context_bridge();
void a2fo_recovery_node_bridge();
void* a2fo_recovery_context_gateway=nullptr;
void* a2fo_recovery_node_gateway=nullptr;
}
namespace {
namespace rt=a2fo::recovery_tests;
constexpr char name[]="A2FORecoveryTests";
const A2FO_ModuleApi* api=nullptr;
struct Config {
    bool enabled=false,engine_context=true,node_transform=true,capture_first_match=true;
    unsigned sample_every=1000,max_samples=1000,max_reports=16;
} config;
struct Counts {volatile LONG visited=0,attempted=0,matched=0,mismatched=0,skipped=0,busy=0;};
Counts counts[2];
volatile LONG armed[2]={0,0},busy=0,reports=0,closing=0;
DWORD session_tick=0;
std::string report_directory;
struct Frame {std::uint32_t admitted;rt::Snapshot snapshot;
#ifdef A2FO_BUILTIN_FEATURE
    std::uint64_t activation;
#endif
};
std::uint64_t runtime_activation() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    const auto gate=[](void*,std::uint32_t) noexcept ->std::uint64_t {return a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE);};
#else
    const auto gate=[](void*,std::uint32_t) noexcept ->std::uint64_t {return 1;};
#endif
    return a2fo_rs_recovery_activation(closing,nullptr,gate);
}
static_assert(sizeof(Frame)<=384,"Bridge frame reservation");
struct LastErrorGuard {
    DWORD value=GetLastError();
    ~LastErrorGuard(){SetLastError(value);}
};

void log(const std::string& message) noexcept {
    try {if(!closing && api && api->log)api->log(name,message.c_str());}catch(...){}
}
bool number(const std::string& path,const char* key,unsigned fallback,unsigned minimum,unsigned maximum,unsigned& value) {
    char buffer[64],default_value[32];std::snprintf(default_value,sizeof(default_value),"%u",fallback);
    const DWORD size=GetPrivateProfileStringA("RecoveryTests",key,default_value,buffer,sizeof(buffer),path.c_str());
    if(!size || size>=sizeof(buffer)-1)return false;
    std::uint32_t parsed=0;
    if(!a2fo_rs_recovery_parse_number(reinterpret_cast<const std::uint8_t*>(buffer),size,minimum,maximum,&parsed))return false;
    value=parsed;return true;
}
bool load_config(const std::string& path,Config& out) {
    Config next;unsigned enabled,context,node,first;
    if(!number(path,"Enabled",0,0,1,enabled) || !number(path,"EngineContext",1,0,1,context) ||
       !number(path,"NodeTransform",1,0,1,node) || !number(path,"CaptureFirstMatch",1,0,1,first) ||
       !number(path,"SampleEvery",1000,1,1000000,next.sample_every) ||
       !number(path,"MaxSamplesPerRoutine",1000,1,1000000,next.max_samples) ||
       !number(path,"MaxReports",16,0,256,next.max_reports))return false;
    next.enabled=enabled;next.engine_context=context;next.node_transform=node;next.capture_first_match=first;
    const A2foRsRecoveryConfig candidate={enabled,context,node,first,next.sample_every,next.max_samples,next.max_reports};
    if(!a2fo_rs_recovery_config_valid(&candidate))return false;
    out=next;return true;
}
bool copy_live(std::uint32_t address,void* out,std::size_t bytes) noexcept {
    SIZE_T read=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(std::uintptr_t(address)),out,bytes,&read) && read==bytes;
}
bool file_sha256(const char* path,std::string& result) {
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
        std::array<BYTE,8192> bytes{};DWORD read=0;bool success=true;
        for(;;) {if(!ReadFile(file,bytes.data(),bytes.size(),&read,nullptr)){success=false;break;}
            if(!read)break;
            if(!CryptHashData(hash,bytes.data(),read,0)){success=false;break;}}
        std::array<BYTE,32> digest{};DWORD size=digest.size();
        if(success && CryptGetHashParam(hash,HP_HASHVAL,digest.data(),&size,0) && size==digest.size()) {
            char text[65];for(unsigned i=0;i<32;++i)std::snprintf(text+i*2,3,"%02x",digest[i]);result=text;ok=true;
        }
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return ok;
}
bool supported_image(HMODULE image) {
    char path[32768];const DWORD n=GetModuleFileNameA(image,path,sizeof(path));
    if(!n || n>=sizeof(path))return false;
    std::string digest;if(!file_sha256(path,digest) || digest!=rt::input_sha256)return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>=4096)return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const char*>(image)+dos->e_lfanew);
    return nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
           nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC && reinterpret_cast<std::uintptr_t>(image)==0x400000;
}
bool exact_body(HMODULE image,std::uint32_t rva,const unsigned char* expected,std::size_t size) {
    std::array<unsigned char,187> current{};
    return size<=current.size() && copy_live(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image))+rva,current.data(),size) &&
           std::memcmp(current.data(),expected,size)==0;
}
void summary(unsigned kind) {
    const auto& c=counts[kind];char text[256];
    std::snprintf(text,sizeof(text),"%s visited=%ld attempted=%ld matched=%ld mismatched=%ld skipped=%ld busy=%ld",
        kind?"node_transform":"engine_context",c.visited,c.attempted,c.matched,c.mismatched,c.skipped,c.busy);log(text);
}
void save_record(const rt::Snapshot& snapshot,const rt::Comparison& comparison) {
    if(!runtime_activation())return;
#ifdef A2FO_BUILTIN_FEATURE
    // Preparation must not create report directories while the module is off.
    if(!CreateDirectoryA(report_directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return;
    const DWORD attributes=GetFileAttributesA(report_directory.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY))return;
#endif
    const LONG index=reports;
    if(index>=static_cast<LONG>(config.max_reports))return;
    std::string text=rt::record_json(snapshot,comparison);
    text.pop_back();text+=",\"input_sha256\":\"";text+=rt::input_sha256;
    text+="\",\"module_build\":\"20260930-recovery-tests-1\"}\n";
    char basename[128];std::snprintf(basename,sizeof(basename),"%s-%s-%lu-%lu-%03ld.json",
        snapshot.routine==rt::Routine::node_transform?"node":"context",comparison.outcome==rt::Outcome::match?"match":"mismatch",
        GetCurrentProcessId(),session_tick,index+1);
    const std::string path=report_directory+"\\"+basename;
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){InterlockedIncrement(&reports);log("Capture file could not be created (existing files are preserved)");return;}
    DWORD written=0;const bool ok=WriteFile(file,text.data(),text.size(),&written,nullptr) && written==text.size();
    CloseHandle(file);InterlockedIncrement(&reports);log(ok?"Captured "+path:"Capture write failed: "+path);
}
// Publish a fully built gateway BEFORE exposing the branch to the thunk.
// This prevents a callback entering an installed hook with a null gateway.
bool install(HMODULE image,unsigned kind,const unsigned char* expected,std::uint32_t rva,void* replacement,void** gateway) {
    auto* target=reinterpret_cast<unsigned char*>(image)+rva;
    auto* code=static_cast<unsigned char*>(VirtualAlloc(nullptr,11,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!code)return false;
    std::memcpy(code,expected,6);code[6]=0xe9;
    const auto displacement=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target)+6-reinterpret_cast<std::uintptr_t>(code)-11);
    std::memcpy(code+7,&displacement,4);DWORD previous=0;
    if(!VirtualProtect(code,11,PAGE_EXECUTE_READ,&previous) || !FlushInstructionCache(GetCurrentProcess(),code,11)){VirtualFree(code,0,MEM_RELEASE);return false;}
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(gateway),code);
    if(!api->patch_jump(target,replacement,expected,6)){InterlockedExchangePointer(reinterpret_cast<void* volatile*>(gateway),nullptr);VirtualFree(code,0,MEM_RELEASE);return false;}
    InterlockedExchange(&armed[kind],1);return true;
}
} // namespace

namespace {
struct BeforeContext {std::uint32_t kind,object;Frame* frame;const unsigned char* fx;};
std::uint64_t before_operation(void* opaque,std::uint32_t op) noexcept {
    auto& b=*static_cast<BeforeContext*>(opaque);auto& frame=*b.frame;
    auto& c=counts[b.kind];
    try {switch(op) {
    case 0:return !closing && armed[b.kind];
    case 1:return static_cast<unsigned>(InterlockedIncrement(&c.visited));
    case 2:return static_cast<unsigned>(c.attempted);
    case 3:if(InterlockedCompareExchange(&busy,1,0)!=0){InterlockedIncrement(&c.busy);return 0;}return 1;
    case 4:return static_cast<unsigned>(c.attempted);
    case 5: {
        frame.admitted=1;frame.snapshot={};auto& s=frame.snapshot;
        s.routine=static_cast<rt::Routine>(b.kind);s.object=b.object;
        std::memcpy(&s.control_word,b.fx,2);s.fp_tag=b.fx[4];
        InterlockedIncrement(&c.attempted);
        bool captured=true;
        if(b.kind==1)captured=copy_live(b.object,s.node_before.data(),s.node_before.size());
        else {
            s.context_addresses[0]=b.object+0xc0;
            captured=copy_live(s.context_addresses[0],&s.context_before[0],4);
            if(captured){s.context_addresses[1]=b.object+s.context_before[0]*4+0xcc;captured=copy_live(s.context_addresses[1],&s.context_before[1],4);}
            if(captured){s.context_addresses[2]=s.context_before[1]+0x28;captured=copy_live(s.context_addresses[2],&s.context_before[2],4);}
        }
        if(!captured)frame.admitted=2;
        return 1;
    }
    case 6:InterlockedExchange(&busy,0);return 1;
    case 7:return config.sample_every;
    case 8:return config.max_samples;
    default:return 0;
    }}catch(...){return 0;}
}
}
extern "C" void __cdecl a2fo_recovery_before(std::uint32_t kind,std::uint32_t object,void* storage,const unsigned char* fx) noexcept {
    LastErrorGuard last_error;
    auto& frame=*static_cast<Frame*>(storage);frame.admitted=0;
#ifdef A2FO_BUILTIN_FEATURE
    frame.activation=runtime_activation();const auto activation=frame.activation;
#else
    const auto activation=runtime_activation();
#endif
    if(kind>1)return;
    BeforeContext context{kind,object,&frame,fx};
    a2fo_rs_recovery_before(kind,activation,&context,before_operation);
}
namespace {
struct AfterContext {Frame* frame;std::uint32_t result;rt::Comparison comparison{};};
std::uint64_t after_operation(void* opaque,std::uint32_t op) noexcept {
    auto& a=*static_cast<AfterContext*>(opaque);auto& frame=*a.frame;
    const auto kind=static_cast<unsigned>(frame.snapshot.routine);auto& c=counts[kind];
    try {switch(op) {
    case 0:return runtime_activation();
    case 1:InterlockedExchange(&busy,0);return 1;
    case 2: {
        auto& s=frame.snapshot;s.actual_return=a.result;
        bool captured=frame.admitted==1;
        if(captured && kind==1)captured=copy_live(s.object,s.node_after.data(),s.node_after.size());
        if(captured && kind==0)for(unsigned i=0;i<3;++i)if(!copy_live(s.context_addresses[i],&s.context_after[i],4)){captured=false;break;}
        if(!captured)return 0;
        a.comparison=rt::compare(s);
        return a.comparison.outcome==rt::Outcome::skip?0:a.comparison.outcome==rt::Outcome::match?1:2;
    }
    case 3:InterlockedIncrement(&c.skipped);return 1;
    case 4:return static_cast<unsigned>(InterlockedIncrement(&c.matched));
    case 5:InterlockedIncrement(&c.mismatched);log(std::string(kind?"node_transform":"engine_context")+" mismatch: "+a.comparison.reason);return 1;
    case 6:save_record(frame.snapshot,a.comparison);return 1;
    case 7:summary(kind);return 1;
    case 9:return static_cast<unsigned>(c.attempted);
    case 10:return config.max_samples;
    case 11:return config.capture_first_match;
    case 12:log("Observer exception contained; original result retained");return 1;
    default:return 0;
    }}catch(...){return 3;}
}
}
extern "C" void __cdecl a2fo_recovery_after(void* storage,std::uint32_t result) noexcept {
    LastErrorGuard last_error;
    auto& frame=*static_cast<Frame*>(storage);
#ifdef A2FO_BUILTIN_FEATURE
    const auto activation=frame.activation;
#else
    const auto activation=runtime_activation();
#endif
    AfterContext context{&frame,result};
    a2fo_rs_recovery_after(frame.admitted,activation,&context,after_operation);
}
struct InitContext {const A2FO_ModuleApi* supplied;std::string root;HMODULE image=nullptr;};
std::uint64_t init_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<InitContext*>(opaque);
    try {switch(op) {
    case 0: {
        return load_config(c.root+"\\A2FORecoveryTests.ini",config);
    }
    case 1:return config.enabled;
    case 2:return config.engine_context || config.node_transform;
    case 3:return api->armada_module && api->patch_jump;
    case 4:return IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)!=0;
    case 5:c.image=static_cast<HMODULE>(api->armada_module());return c.image && supported_image(c.image);
    case 6:return (!config.engine_context || exact_body(c.image,rt::context_rva,rt::context_body,sizeof(rt::context_body))) &&
                  (!config.node_transform || exact_body(c.image,rt::node_rva,rt::node_body,sizeof(rt::node_body)));
    case 7: {
        report_directory=c.root+"\\A2FORecoveryTests";session_tick=GetTickCount();
#ifndef A2FO_BUILTIN_FEATURE
        if(!CreateDirectoryA(report_directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS){log("Report directory unavailable; passive");return 0;}
        const DWORD attributes=GetFileAttributesA(report_directory.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY)){log("Report path is not a directory; passive");return 0;}
#endif
        return 1;
    }
    case 8: {HMODULE resident=nullptr;return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(&a2fo_recovery_context_bridge),&resident)!=0;}
    case 9: {HMODULE host=nullptr;return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(api->log),&host)!=0;}
    case 10:log("Invalid configuration; passive, no hooks installed");return 1;
    case 11:log("Disabled; passive, no hooks installed");return 1;
    case 12:log("No routines enabled; passive");return 1;
    case 13:log("Required SDK hook support missing; passive");return 1;
    case 14:log("FXSAVE/SSE support unavailable; passive");return 1;
    case 15:log("Pinned EXE SHA-256/base/PE identity mismatch; passive");return 1;
    case 16:log("Complete routine bytes differ or another hook owns a target; passive");return 1;
    case 17:return 1; // Directory-specific message was emitted by native operation 7.
    case 18:log("Could not pin module lifetime; passive");return 1;
    case 19:log("Could not pin SDK host lifetime; passive");return 1;
    case 20:return config.engine_context;
    case 21:return config.node_transform;
    case 22:return install(c.image,0,rt::context_body,rt::context_rva,reinterpret_cast<void*>(&a2fo_recovery_context_bridge),&a2fo_recovery_context_gateway);
    case 23:return install(c.image,1,rt::node_body,rt::node_rva,reinterpret_cast<void*>(&a2fo_recovery_node_bridge),&a2fo_recovery_node_gateway);
    case 24:log("Observation hooks installed; originals retained, isolated replay only");return 1;
    case 25:log("Partial hook installation; installed observers retained safely, failed targets untouched");return 1;
    default:return 0;
    }}catch(...){return 0;}
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* supplied) {
    const bool valid=supplied && supplied->api_version==A2FO_MODULE_API_VERSION &&
       supplied->struct_size>=offsetof(A2FO_ModuleApi,patch_jump)+sizeof(supplied->patch_jump) &&
       supplied->root_directory && supplied->log;
    if(!valid)return false;
    InitContext context{supplied,{},{}};
    api=supplied;
    const char* root=api->root_directory();if(!root || !*root)return false;
    context.root=root;
    return a2fo_rs_recovery_init(1,&context,init_operation)!=0;
}
std::uint64_t lifecycle_operation(void*,std::uint32_t op) noexcept {
    switch(op) {
    case 0:summary(0);return 1;
    case 1:summary(1);return 1;
    case 2:InterlockedExchange(&closing,1);return 1;
    case 3:InterlockedExchange(&armed[0],0);return 1;
    case 4:InterlockedExchange(&armed[1],0);return 1;
    default:return 0;
    }
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FORecoveryTests_Status() {
    a2fo_rs_recovery_status(runtime_activation(),nullptr,lifecycle_operation);
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    a2fo_rs_recovery_shutdown(nullptr,lifecycle_operation);
    // Gateways and pinned module remain resident; no live branch points to freed code.
}
