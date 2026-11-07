// Host-only Win32 mock backend for the historical stage-five transcript.
// Production never compiles this file or the retired C++ interface fixture.
#include <windows.h>
#include "../rust/native_hooks_ffi.h"
namespace {
int32_t protect(void* p,size_t n,uint32_t flags,uint32_t* previous){DWORD old=0;const auto result=VirtualProtect(p,n,flags,&old);*previous=old;return result;}
void* allocate(size_t n){return VirtualAlloc(nullptr,n,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);}
void release(void* p){VirtualFree(p,0,MEM_RELEASE);}
void flush(void* p,size_t n){FlushInstructionCache(GetCurrentProcess(),p,n);}
const A2foRsHookMemory memory{protect,allocate,release,flush};
}
extern "C" uint8_t a2fo_rs_windows_hook_patch(void* target,const void* replacement,const uint8_t* expected,size_t n,uint32_t kind){return a2fo_rs_hook_patch(target,replacement,expected,n,kind,&memory);}
extern "C" uint8_t a2fo_rs_windows_hook_install(void* target,void* replacement,size_t n,const uint8_t* expected,A2foRsInlineHook* out){return a2fo_rs_hook_install(target,replacement,n,expected,out,&memory);}
