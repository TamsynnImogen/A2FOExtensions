// Private Win32/Wine observer fixture. The pinned routines execute from
// executable copies; no installed game image or input device is touched.
#include "../modules/A2FORecoveryTests/module.cpp"
#include "../rust/native_hooks_ffi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

extern "C" {
alignas(16) unsigned char recovery_probe_saved_fx[512] = {};
}

namespace {
struct alignas(16) Probe {
    std::uint32_t result, ebx, esi, edi, ebp, esp, flags, ecx, edx, pre_esp;
    std::uint32_t padding[2]{};
    alignas(16) std::array<std::uint8_t,512> fx{};
};
static_assert(offsetof(Probe, fx)==48);
extern "C" void recovery_probe(void* function,void* object,const void* fx_seed,
                                 Probe* result,std::uint32_t flags);

void require(bool value,const char* reason) {
    if(!value)throw std::runtime_error(reason);
}
std::uint32_t bits(float value) {std::uint32_t word;std::memcpy(&word,&value,4);return word;}
std::uint32_t word(const void* address) {std::uint32_t value;std::memcpy(&value,address,4);return value;}
void put(void* address,std::uint32_t value) {std::memcpy(address,&value,4);}
struct Object {
    std::array<std::uint8_t,16> before{};
    std::array<std::uint8_t,0x100> data{};
    std::array<std::uint8_t,16> after{};
    Object() {before.fill(0xa5);data.fill(0x6b);after.fill(0x5a);}
    void check() const {
        for(auto byte:before)require(byte==0xa5,"object prefix");
        for(auto byte:after)require(byte==0x5a,"object suffix");
    }
};

void* copy_body(const unsigned char* bytes,std::size_t size) {
    void* memory=VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    require(memory!=nullptr,"VirtualAlloc original copy");
    std::memcpy(memory,bytes,size);
    DWORD previous=0;
    require(VirtualProtect(memory,size,PAGE_EXECUTE_READ,&previous)!=0,"protect original copy");
    require(FlushInstructionCache(GetCurrentProcess(),memory,size)!=0,"flush original copy");
    return memory;
}

std::array<std::uint8_t,512> seed(std::uint16_t control) {
    alignas(16) std::array<std::uint8_t,512> output{};
    asm volatile("fninit\n\tfldcw %1\n\tfxsave %0" : "=m"(output) : "m"(control) : "memory");
    return output;
}

void check_probe(const Probe& actual,const Probe& direct,std::uint32_t wanted) {
    require(actual.result==wanted && direct.result==wanted,"return value");
    for(const auto* p:{&actual,&direct}) {
        require(p->ebx==0x12345678 && p->esi==0x23456789 &&
                p->edi==0x3456789a,"nonvolatile registers");
        require(p->esp==p->pre_esp,"stack pointer");
    }
    require(actual.ebp==direct.ebp,"frame pointer");
    require(actual.flags==direct.flags,"EFLAGS passthrough");
    require(actual.ecx==direct.ecx && actual.edx==direct.edx,"volatile register equivalence");
    require(actual.fx==direct.fx,"full FXSAVE passthrough");
}

void fake_log(const char*,const char* message);
std::vector<std::string> logs;
std::string root_path;
long patch_calls=0;
A2FO_ModuleApi sdk{};
void fake_log(const char*,const char* message) {logs.emplace_back(message);}
void* fake_armada_null() {return nullptr;}
void* fake_armada_self() {return GetModuleHandleA(nullptr);}
const char* fake_root() {return root_path.c_str();}
bool fake_patch(void* target,void* replacement,const std::uint8_t* expected,std::size_t size) {
    ++patch_calls;
    return a2fo_rs_windows_patch_jump(target,replacement,expected,size);
}
bool has_log(const char* text) {
    for(const auto& line:logs)if(line.find(text)!=std::string::npos)return true;
    return false;
}
void write_ini(const std::string& folder,const char* content) {
    std::ofstream out(folder+"\\A2FORecoveryTests.ini",std::ios::binary);
    require(bool(out),"INI open");out<<content;require(bool(out),"INI write");
}
void init_cases(const std::string& temp) {
    sdk={};
    sdk.struct_size=sizeof(sdk);sdk.api_version=A2FO_MODULE_API_VERSION;
    sdk.log=fake_log;sdk.root_directory=fake_root;
    sdk.armada_module=fake_armada_null;sdk.patch_jump=fake_patch;
    root_path=temp+"\\disabled";CreateDirectoryA(root_path.c_str(),nullptr);
    write_ini(root_path,"[RecoveryTests]\r\nEnabled=0\r\n");
    closing=0;logs.clear();require(A2FO_ModuleInit(&sdk),"disabled init");
    require(has_log("Disabled; passive") && patch_calls==0,"disabled made no hooks");
    A2FO_ModuleShutdown();
    root_path=temp+"\\unsupported";CreateDirectoryA(root_path.c_str(),nullptr);
    write_ini(root_path,"[RecoveryTests]\r\nEnabled=1\r\nSampleEvery=1\r\n");
    closing=0;logs.clear();require(A2FO_ModuleInit(&sdk),"unsupported init");
    require(has_log("identity mismatch") && patch_calls==0,"null image passive");
    A2FO_ModuleShutdown();
    sdk.armada_module=fake_armada_self;
    closing=0;logs.clear();require(A2FO_ModuleInit(&sdk),"hash guard init");
    require(has_log("identity mismatch") && patch_calls==0,"wrong image SHA passive");
    A2FO_ModuleShutdown();
    sdk.api_version=0;
    require(!A2FO_ModuleInit(&sdk),"API major version rejection");
}
void delivered_dll_case(const std::string& temp) {
    root_path=temp+"\\dll-disabled";CreateDirectoryA(root_path.c_str(),nullptr);
    write_ini(root_path,"[RecoveryTests]\r\nEnabled=0\r\n");
    sdk.api_version=A2FO_MODULE_API_VERSION;
    logs.clear();
    HMODULE dll=LoadLibraryA("build\\modules\\A2FORecoveryTests.dll");
    require(dll!=nullptr,"load delivered recovery DLL");
    using Init=bool (__cdecl*)(const A2FO_ModuleApi*);
    using Void=void (__cdecl*)();
    static_assert(sizeof(FARPROC)==sizeof(Init) && sizeof(FARPROC)==sizeof(Void));
    const FARPROC init_symbol=GetProcAddress(dll,"A2FO_ModuleInit");
    const FARPROC shutdown_symbol=GetProcAddress(dll,"A2FO_ModuleShutdown");
    const FARPROC status_symbol=GetProcAddress(dll,"A2FORecoveryTests_Status");
    Init dll_init=nullptr;Void dll_shutdown=nullptr,dll_status=nullptr;
    std::memcpy(&dll_init,&init_symbol,sizeof(dll_init));
    std::memcpy(&dll_shutdown,&shutdown_symbol,sizeof(dll_shutdown));
    std::memcpy(&dll_status,&status_symbol,sizeof(dll_status));
    require(dll_init && dll_shutdown && dll_status,"delivered DLL exports");
    const long before=patch_calls;
    require(dll_init(&sdk),"delivered DLL disabled init");
    require(has_log("Disabled; passive") && patch_calls==before,"delivered DLL passive");
    dll_status();dll_shutdown();
    require(FreeLibrary(dll)!=0,"unload passive delivered DLL");
}

void context_case(void* original,const std::array<std::uint8_t,512>& fx,
                  std::uint32_t flags,void* observed_entry=nullptr) {
    Object engine,selected;
    constexpr std::uint32_t value=0x9abc1234;
    put(engine.data.data()+0xc0,1);
    put(engine.data.data()+0xd0,reinterpret_cast<std::uint32_t>(selected.data.data()));
    put(selected.data.data()+0x28,value);
    const auto before_engine=engine.data,before_selected=selected.data;
    Probe direct{},actual{};
    SetLastError(0xa2f00001u);
    recovery_probe(original,engine.data.data(),fx.data(),&direct,flags);
    require(GetLastError()==0xa2f00001u,"context direct LastError");
    require(engine.data==before_engine && selected.data==before_selected,"context direct unchanged");
    SetLastError(0xa2f00002u);
    recovery_probe(observed_entry?observed_entry:reinterpret_cast<void*>(&a2fo_recovery_context_bridge),
                   engine.data.data(),fx.data(),&actual,flags);
    require(GetLastError()==0xa2f00002u,"context observed LastError");
    check_probe(actual,direct,value);
    require(engine.data==before_engine && selected.data==before_selected,"context objects unchanged");
    engine.check();selected.check();
}

void node_case(void* original,const std::array<std::uint8_t,512>& fx,
               std::uint32_t flags,bool unit,void* observed_entry=nullptr) {
    Object node;
    for(unsigned i=0;i<9;++i)put(node.data.data()+0x24+i*4,bits(float(int(i)-4)*0.5f));
    for(unsigned i=0;i<3;++i)put(node.data.data()+0x48+i*4,0x7fc00001u+i);
    put(node.data.data()+0x54,bits(unit?1.0f:2.0f));
    const auto initial=node.data;
    Probe direct{},actual{};
    SetLastError(0xa2f00003u);
    recovery_probe(original,node.data.data(),fx.data(),&direct,flags);
    require(GetLastError()==0xa2f00003u,"node direct LastError");
    const auto direct_image=node.data;
    node.data=initial;
    SetLastError(0xa2f00004u);
    recovery_probe(observed_entry?observed_entry:reinterpret_cast<void*>(&a2fo_recovery_node_bridge),
                   node.data.data(),fx.data(),&actual,flags);
    require(GetLastError()==0xa2f00004u,"node observed LastError");
    const auto wanted=reinterpret_cast<std::uint32_t>(node.data.data())+(unit?0x24u:0x58u);
    require(direct.result==wanted,"original node return");
    require(actual.result==wanted,"observed node return");
    for(const auto* p:{&direct,&actual}) {
        require(p->ebx==0x12345678 && p->esi==0x23456789 && p->edi==0x3456789a,"node nonvolatile");
        require(p->esp==p->pre_esp,"node ESP");
    }
    require(actual.ebp==direct.ebp && actual.flags==direct.flags &&
            actual.ecx==direct.ecx && actual.edx==direct.edx && actual.fx==direct.fx,
            "node frame/volatile/flags/FXSAVE equivalence");
    require(direct_image==node.data,"node whole-object equivalence");
    if(unit)require(node.data==initial,"unit path unchanged");
    else {
        for(unsigned i=0;i<9;++i)
            require(word(node.data.data()+0x58+i*4)==bits(float(int(i)-4)),"scaled rotation");
        for(unsigned i=0;i<3;++i)
            require(word(node.data.data()+0x7c+i*4)==0x7fc00001u+i,"raw translation copy");
    }
    node.check();
}
} // namespace

int main() {
    try {
        char temp_base[MAX_PATH]{};
        require(GetTempPathA(MAX_PATH,temp_base)!=0,"GetTempPath");
        const std::string temp=std::string(temp_base)+"a2fo-recovery-fixture-"+
            std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount());
        require(CreateDirectoryA(temp.c_str(),nullptr)!=0 || GetLastError()==ERROR_ALREADY_EXISTS,"fixture temp directory");
        init_cases(temp);
        delivered_dll_case(temp);
        void* context_original=copy_body(a2fo::recovery_tests::context_body,sizeof(a2fo::recovery_tests::context_body));
        void* node_original=copy_body(a2fo::recovery_tests::node_body,sizeof(a2fo::recovery_tests::node_body));
        a2fo_recovery_context_gateway=context_original;
        a2fo_recovery_node_gateway=node_original;
        armed[0]=armed[1]=1;busy=0;reports=0;closing=0;
        for(auto& count:counts) {
            count.visited=count.attempted=count.matched=count.mismatched=count.skipped=count.busy=0;
        }
        config.sample_every=1;config.max_samples=1000;config.max_reports=0;
        config.capture_first_match=false;
        std::uint32_t context_cases=0,node_cases=0;
        for(const std::uint16_t mode:{std::uint16_t(0x007f),std::uint16_t(0x027f),std::uint16_t(0x037f)}) {
            const auto fp=seed(mode);
            for(const std::uint32_t flags:{0x246u,0x646u}) {
                context_case(context_original,fp,flags);++context_cases;
                node_case(node_original,fp,flags,true);++node_cases;
                node_case(node_original,fp,flags,false);++node_cases;
            }
        }
        require(counts[0].matched==static_cast<LONG>(context_cases) &&
                counts[1].matched==static_cast<LONG>(node_cases),"observer match counts");
        require(counts[0].mismatched==0 && counts[1].mismatched==0 &&
                counts[0].skipped==0 && counts[1].skipped==0,"observer mismatch/skip counts");
        A2FORecoveryTests_Status();
        require(has_log("visited=6 attempted=6 matched=6"),"context summary");
        require(has_log("visited=12 attempted=12 matched=12"),"node summary");
        armed[0]=armed[1]=0;
        const auto context_matches=counts[0].matched,node_matches=counts[1].matched;
        context_case(context_original,seed(0x037f),0x246);
        node_case(node_original,seed(0x037f),0x246,false);
        require(counts[0].matched==context_matches && counts[1].matched==node_matches,
                "disabled observation passthrough");
        armed[0]=1;
        config.max_samples=static_cast<unsigned>(counts[0].attempted);
        const LONG capped_visited=counts[0].visited,capped_attempted=counts[0].attempted;
        context_case(context_original,seed(0x037f),0x246);
        require(counts[0].visited==capped_visited+1 && counts[0].attempted==capped_attempted,
                "sample cap passthrough");
        config.max_samples=1000;
        busy=1;
        const LONG busy_before=counts[0].busy;
        context_case(context_original,seed(0x037f),0x246);
        require(counts[0].busy==busy_before+1 && counts[0].attempted==capped_attempted,
                "busy passthrough");
        busy=0;
        void* context_detour=copy_body(a2fo::recovery_tests::context_body,
                                       sizeof(a2fo::recovery_tests::context_body));
        void* node_detour=copy_body(a2fo::recovery_tests::node_body,
                                    sizeof(a2fo::recovery_tests::node_body));
        require(install(reinterpret_cast<HMODULE>(reinterpret_cast<std::uintptr_t>(context_detour)-
                        a2fo::recovery_tests::context_rva),0,a2fo::recovery_tests::context_body,
                        a2fo::recovery_tests::context_rva,
                        reinterpret_cast<void*>(&a2fo_recovery_context_bridge),
                        &a2fo_recovery_context_gateway),"private context detour install");
        require(install(reinterpret_cast<HMODULE>(reinterpret_cast<std::uintptr_t>(node_detour)-
                        a2fo::recovery_tests::node_rva),1,a2fo::recovery_tests::node_body,
                        a2fo::recovery_tests::node_rva,
                        reinterpret_cast<void*>(&a2fo_recovery_node_bridge),
                        &a2fo_recovery_node_gateway),"private node detour install");
        require(patch_calls==2,"checked patch calls");
        context_case(context_original,seed(0x037f),0x246,context_detour);
        node_case(node_original,seed(0x037f),0x246,true,node_detour);
        node_case(a2fo_recovery_node_gateway,seed(0x037f),0x246,false,node_detour);
        require(counts[0].matched==context_matches+1 && counts[1].matched==node_matches+2,
                "installed detour/gateway matched");
        report_directory=temp;
        reports=0;counts[0].matched=0;
        config.max_reports=1;config.capture_first_match=true;
        context_case(context_original,seed(0x037f),0x246,context_detour);
        const std::string pattern=temp+"\\context-match-*-001.json";
        WIN32_FIND_DATAA found{};
        HANDLE capture_find=FindFirstFileA(pattern.c_str(),&found);
        require(capture_find!=INVALID_HANDLE_VALUE,"first-match capture created");
        FindClose(capture_find);
        const std::string capture=temp+"\\"+found.cFileName;
        std::ifstream saved(capture,std::ios::binary);
        require(bool(saved),"first-match capture created");
        std::string contents((std::istreambuf_iterator<char>(saved)),std::istreambuf_iterator<char>());
        require(contents.find("\"outcome\":\"match\"")!=std::string::npos,"capture content");
        require(reports==1,"capture count");
        Object mismatch_engine,mismatch_selected;
        const auto mismatch_object=reinterpret_cast<std::uint32_t>(mismatch_engine.data.data());
        const auto selected_object=reinterpret_cast<std::uint32_t>(mismatch_selected.data.data());
        put(mismatch_engine.data.data()+0xc0,0);
        put(mismatch_engine.data.data()+0xcc,selected_object);
        put(mismatch_selected.data.data()+0x28,0x13572468u);
        Frame frame{};
        frame.admitted=1;
        frame.snapshot.routine=rt::Routine::engine_context;
        frame.snapshot.object=mismatch_object;
        frame.snapshot.context_addresses={mismatch_object+0xc0u,mismatch_object+0xccu,
                                          selected_object+0x28u};
        frame.snapshot.context_before={0,selected_object,0x13572468u};
        reports=0;busy=1;config.capture_first_match=false;
        const LONG mismatch_before=counts[0].mismatched;
        a2fo_recovery_after(&frame,0x13572469u);
        require(counts[0].mismatched==mismatch_before+1 && busy==0 && reports==1,
                "injected mismatch recorded");
        const std::string mismatch_pattern=temp+"\\context-mismatch-*-001.json";
        WIN32_FIND_DATAA mismatch_found{};
        HANDLE mismatch_find=FindFirstFileA(mismatch_pattern.c_str(),&mismatch_found);
        require(mismatch_find!=INVALID_HANDLE_VALUE,"mismatch capture created");
        FindClose(mismatch_find);
        std::ifstream mismatch_saved(temp+"\\"+mismatch_found.cFileName,std::ios::binary);
        std::string mismatch_text((std::istreambuf_iterator<char>(mismatch_saved)),
                                  std::istreambuf_iterator<char>());
        require(mismatch_text.find("\"outcome\":\"mismatch\"")!=std::string::npos,
                "mismatch capture content");
        mismatch_engine.check();mismatch_selected.check();
        armed[0]=armed[1]=0;
        require(VirtualFree(context_original,0,MEM_RELEASE)!=0 &&
                VirtualFree(node_original,0,MEM_RELEASE)!=0 &&
                VirtualFree(context_detour,0,MEM_RELEASE)!=0 &&
                VirtualFree(node_detour,0,MEM_RELEASE)!=0,"free private copies");
        std::cout<<"{\"status\":\"PASS\",\"context_cases\":"<<context_cases
                 <<",\"node_cases\":"<<node_cases
                 <<",\"precision_modes\":3,\"flags_modes\":2,\"matched\":"
                 <<(context_matches+node_matches)<<",\"detour_cases\":3,\"captures\":2,\"capture_path\":";
        std::cout<<'"';for(char character:capture){if(character=='\\' || character=='"')std::cout<<'\\';std::cout<<character;}
        std::cout<<"\"}\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"recovery fixture: "<<error.what()<<'\n';
        return 1;
    }
}
