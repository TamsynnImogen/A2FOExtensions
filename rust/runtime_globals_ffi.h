#pragma once
#include <windows.h>
#include <cstddef>
struct A2foRuntimeGlobals {
    volatile LONG initialize_state;
    volatile LONG deferred_finished;
    volatile DWORD initialize_thread_id;
    HANDLE log;
};
static_assert(sizeof(LONG)==4 && sizeof(DWORD)==4,"Win32 lifecycle cell width");
static_assert(offsetof(A2foRuntimeGlobals,initialize_state)==0 &&
              offsetof(A2foRuntimeGlobals,deferred_finished)==4 &&
              offsetof(A2foRuntimeGlobals,initialize_thread_id)==8,"lifecycle layout");
#if !defined(_WIN64)
static_assert(sizeof(A2foRuntimeGlobals)==16 && offsetof(A2foRuntimeGlobals,log)==12,"i686 lifecycle layout");
#endif
extern "C" A2foRuntimeGlobals* a2fo_rs_runtime_globals();
