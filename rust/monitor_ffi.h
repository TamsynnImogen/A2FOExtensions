#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsMonitor {char name[128],label[128];uintptr_t handle;uint32_t flags;int32_t index,left,top,width,height,work_left,work_top,work_right,work_bottom;} A2foRsMonitor;
typedef int32_t (*A2foRsMonitorVisit)(void*,uintptr_t);
typedef struct A2foRsMonitorApi {void* context;int32_t (*action)(void*,uint32_t,uintptr_t,uintptr_t,A2foRsMonitor*,const char*,const char*);void (*enumerate)(void*,A2foRsMonitorVisit,void*);} A2foRsMonitorApi;
void a2fo_rs_monitor_list(const A2foRsMonitorApi*);
uint8_t a2fo_rs_monitor_read(const A2foRsMonitorApi*,const uint8_t*,size_t);
uint8_t a2fo_rs_monitor_write(const A2foRsMonitorApi*,const uint8_t*,size_t,const uint8_t*,size_t);
uint8_t a2fo_rs_monitor_window(const A2foRsMonitorApi*,uintptr_t,const uint8_t*,size_t,uint8_t);
#ifdef __cplusplus
}
#endif
