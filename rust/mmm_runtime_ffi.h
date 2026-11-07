#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* All callbacks catch C++ exceptions and return failed=1. Rust never unwinds. */
typedef struct A2FO_MmmResult { uintptr_t value; uint32_t failed; } A2FO_MmmResult;
typedef A2FO_MmmResult (*A2FO_MmmInvoke)(void*, uint32_t, void*);
enum A2FO_MmmOperation {
 A2FO_MMM_LOG=1, A2FO_MMM_EDIT_MODE, A2FO_MMM_RESET_FINISH,
 A2FO_MMM_PENDING_FINISH, A2FO_MMM_SET_INTERFACE, A2FO_MMM_HAS_INTERFACE,
 A2FO_MMM_MONITOR, A2FO_MMM_CREATE_WORKER, A2FO_MMM_DESTROY_WORKER,
 A2FO_MMM_START_WORKER, A2FO_MMM_RESUME_WORKER
};
enum A2FO_MmmLog {
 A2FO_MMM_EXTEND_ENTERED=1, A2FO_MMM_NULL_INTERFACE, A2FO_MMM_RESET_CHECKED,
 A2FO_MMM_EXTEND_EDIT_SKIP, A2FO_MMM_EDIT_CHECKED, A2FO_MMM_RELOAD_DEFERRED,
 A2FO_MMM_INTERFACE_STORED, A2FO_MMM_ATTACHED, A2FO_MMM_INIT_ENTERED,
 A2FO_MMM_INIT_EDIT_SKIP, A2FO_MMM_INIT_NO_INTERFACE, A2FO_MMM_MONITOR_BEGIN,
 A2FO_MMM_MONITOR_DONE, A2FO_MMM_FINISH_HANDED, A2FO_MMM_FIRST_INPUT,
 A2FO_MMM_CONSTRUCTING, A2FO_MMM_CONSTRUCTED, A2FO_MMM_STARTING,
 A2FO_MMM_STARTED, A2FO_MMM_RESUMING, A2FO_MMM_RESUMED
};
void* a2fo_rs_mmm_runtime(void);
A2FO_MmmResult a2fo_rs_mmm_extend(void*, A2FO_MmmInvoke, void*, void*);
A2FO_MmmResult a2fo_rs_mmm_init(void*, A2FO_MmmInvoke, void*);
A2FO_MmmResult a2fo_rs_mmm_input(void*, A2FO_MmmInvoke, void*);
void a2fo_rs_mmm_mark_unloaded(void*);
void a2fo_rs_mmm_abandon_worker(void*);
A2FO_MmmResult a2fo_rs_mmm_drop_worker(void*, A2FO_MmmInvoke, void*);
#ifdef __cplusplus
}
#endif
