#pragma once
#include <cstddef>
#include <cstdint>

// Private byte/POD renderer decisions. Native C++ owns COM and storage.
struct A2foBackendRect { int32_t left, top, right, bottom; };
struct A2foBackendPoint { int32_t x, y; };
struct A2foBackendRegion {
  uint32_t left, top, right, bottom, x, y;
};
extern "C" {
bool a2fo_backend_dimensions(uint32_t width, uint32_t height,
                             uint32_t maximum);
uint32_t a2fo_backend_format(uint32_t legacy, bool depth);
bool a2fo_backend_lost(int32_t status, int32_t removed, int32_t reset);
uint64_t a2fo_backend_remaining(uint64_t budget, uint64_t used);
uint32_t a2fo_backend_available(bool local_ok, uint64_t local_budget,
    uint64_t local_used, bool nonlocal_ok, uint64_t nonlocal_budget,
    uint64_t nonlocal_used);
bool a2fo_backend_copy_region(A2foBackendRect rect, A2foBackendPoint point,
    uint32_t source_width, uint32_t source_height,
    uint32_t destination_width, uint32_t destination_height,
    bool same_subresource, A2foBackendRegion *out);
A2foBackendRect a2fo_backend_clear_bounds(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, uint32_t target_width,
    uint32_t target_height);
A2foBackendRect a2fo_backend_intersect(A2foBackendRect a,
                                        A2foBackendRect b);
bool a2fo_backend_whole_clear(const A2foBackendRect *areas, size_t count,
                              uint32_t target_width, uint32_t target_height);
bool a2fo_backend_mip_compatible(uint32_t format, uint32_t first_format,
    uint32_t width, uint32_t height, uint32_t previous_width,
    uint32_t previous_height);
bool a2fo_backend_draw_viewport(uint32_t x, uint32_t y, uint32_t width,
    uint32_t height, uint32_t target_width, uint32_t target_height);
void a2fo_backend_copy_rows(uint8_t *dst, const uint8_t *src,
    size_t row_bytes, size_t row_pitch, uint32_t height);
}
using A2foBackendCopyInput = void (*)(void *context, uint32_t index,
                                      A2foBackendRect *rect,
                                      A2foBackendPoint *point);
extern "C" bool a2fo_backend_copy_plan(void *context, uint32_t count,
    uint32_t source_width, uint32_t source_height,
    uint32_t destination_width, uint32_t destination_height,
    bool same_subresource, A2foBackendCopyInput input,
    A2foBackendRegion *regions);
using A2foBackendClearInput = void (*)(void *context, uint32_t index,
                                       A2foBackendRect *rect);
extern "C" uint32_t a2fo_backend_clear_plan(void *context,
    A2foBackendRect bounds, uint32_t count, A2foBackendClearInput input,
    A2foBackendRect *areas, bool *valid);
using A2foBackendPrepareMip = void (*)(void *context, uint32_t index,
    int32_t *result, uint32_t *format, uint32_t *width,
    uint32_t *height, bool *threw);
extern "C" int32_t a2fo_backend_prepare_mips(void *context,
    uint32_t count, uint32_t maximum, A2foBackendPrepareMip prepare,
    int32_t invalid_call, int32_t exception_result, bool *threw);
extern "C" bool a2fo_backend_mip_count_valid(uint32_t count,uint32_t maximum);
struct A2foObserverPeLayout {
  uint32_t dos_size,nt_size,import_size,thunk_size;
  uint32_t dos_magic_at,dos_lfanew_at,nt_signature_at,nt_magic_at,
           nt_machine_at,nt_image_size_at,nt_import_rva_at,nt_import_size_at,
           import_name_at,import_original_at,import_first_at;
  uint32_t dos_magic,nt_signature,nt_magic,machine,ordinal_mask;
};
struct A2foObserverPatchPlan {
  uintptr_t slot,original;
  uint32_t duplicate;
};
using A2foObserverSpan=bool (*)(void *context,uintptr_t address,size_t bytes);
using A2foObserverRead=void (*)(void *context,uintptr_t address,uint8_t *out,size_t bytes);
extern "C" bool a2fo_observer_patch_walk(uintptr_t base,
    const A2foObserverPeLayout *layout,A2foObserverSpan span,
    A2foObserverRead read,void *context,uintptr_t callback,
    uintptr_t previous_factory,A2foObserverPatchPlan *plan);
using A2foObserverStep=bool (*)(void *context);
using A2foObserverStage=uint32_t (*)(void *context);
using A2foObserverVoid=void (*)(void *context);
extern "C" void a2fo_observer_observe_workflow(void *context,
    A2foObserverStep prepare,A2foObserverStage prior,
    A2foObserverVoid erase_prior,A2foObserverStage count,
    A2foObserverVoid dropped,A2foObserverStage cloned,
    A2foObserverStep readable_table,A2foObserverStep cache_allowed,
    A2foObserverStep ensure_dispatch,A2foObserverStep publish);
using A2foBackendNativeStep=void (*)(void *context,uint32_t step,
                                     int32_t *result,bool *threw);
extern "C" int32_t a2fo_backend_resize_workflow(void *context,
    uint32_t width,uint32_t height,bool windowed,uint32_t depth_format,
    uint32_t maximum,int32_t invalid_call,int32_t invalid_argument,
    int32_t device_removed,int32_t device_reset,
    A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_observer_iid_kind(const uint8_t *iid16,
    const uint8_t *known16x10);
extern "C" int32_t a2fo_observer_texture_kind(uint32_t resource_type);
extern "C" bool a2fo_observer_trace_file_limit(size_t used,size_t length,
    size_t maximum,size_t margin);
extern "C" bool a2fo_observer_event_sample(bool sampled,bool failed,
    uint32_t events,uint32_t maximum);
extern "C" bool a2fo_observer_present_summary(uint32_t frames);
extern "C" uint32_t a2fo_observer_backend_mode(const uint8_t *name,size_t length,
    bool child_selected);
using A2foObserverDiagnosticStep=bool (*)(void *context,uint32_t step);
using A2foObserverDiagnosticState=uint32_t (*)(void *context);
extern "C" void a2fo_observer_diagnostic_workflow(void *context,
    bool failed,uint32_t events,uint32_t maximum,
    A2foObserverDiagnosticStep step,A2foObserverDiagnosticState state);
extern "C" int32_t a2fo_backend_initialize_workflow(void *context,
    uint32_t width,uint32_t height,uint32_t depth_format,uint32_t maximum,
    bool debug,int32_t invalid_call,int32_t invalid_argument,
    int32_t sdk_component_missing,A2foBackendNativeStep native_step,
    bool *threw);
extern "C" int32_t a2fo_backend_present_workflow(void *context,
    bool source,bool destination,bool override_mismatch,bool dirty,
    int32_t invalid_call,int32_t removed,int32_t reset,
    A2foBackendNativeStep native_step,bool *threw);
struct A2foBackendMemoryPlan {uint32_t available,report_flags;};
extern "C" A2foBackendMemoryPlan a2fo_backend_memory_workflow(
    int32_t result,int32_t local_status,int32_t nonlocal_status,
    uint64_t local_budget,uint64_t local_used,
    uint64_t nonlocal_budget,uint64_t nonlocal_used,
    bool unknown_logged,bool log_only);
extern "C" void a2fo_observer_probe_workflow(void *context,
    bool enabled,bool attempted,A2foObserverStep find,
    A2foObserverStep windowed,A2foObserverStep claim,
    A2foObserverStep client,A2foObserverVoid skipped,
    A2foObserverVoid run);
using A2foObserverBridge=bool (*)(void *context,uint32_t step);
extern "C" bool a2fo_observer_bridge_workflow(void *context,
    bool installed,bool bc_assets,bool fleet_present,
    A2foObserverBridge bridge);
using A2foBackendClearSetupStep=void (*)(void *context,uint32_t step,
    uint32_t index,int32_t *result,bool *threw);
extern "C" int32_t a2fo_backend_clear_setup_workflow(void *context,
    bool already_ready,A2foBackendClearSetupStep step,bool *threw);
extern "C" int32_t a2fo_backend_make_back_workflow(void *context,
    bool depth,A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_create_target_workflow(void *context,
    uint32_t width,uint32_t height,uint32_t legacy,bool depth,uint32_t maximum,
    int32_t invalid_call,A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_bind_target_workflow(void *context,
    bool has_color,bool has_depth,int32_t invalid_call,
    A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_create_texture_workflow(void *context,
    A2foBackendNativeStep native_step,bool *threw);
struct A2foBackendUpdateMeta {
  uint32_t desc_width,desc_height,expected_format,prepared_format;
  uint32_t upload_width,upload_height;
};
extern "C" int32_t a2fo_backend_update_texture_workflow(void *context,
    uint32_t level,uint32_t mip_width,uint32_t mip_height,
    A2foBackendUpdateMeta *meta,int32_t invalid_call,
    A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_texture_view_workflow(void *context,
    int32_t invalid_call,A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_texture_target_workflow(void *context,
    int32_t invalid_call,A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_draw_workflow(void *context,
    uint32_t x,uint32_t y,uint32_t width,uint32_t height,
    uint32_t *target_width,uint32_t *target_height,int32_t invalid_call,
    A2foBackendNativeStep native_step,bool *threw);
extern "C" int32_t a2fo_backend_read_target_workflow(void *context,
    int32_t invalid_call,A2foBackendNativeStep native_step,bool *threw);
extern "C" uint32_t a2fo_backend_status_plan(bool device,bool context,
    bool swap,bool same_thread,bool device_lost);
extern "C" bool a2fo_backend_clear_admission(uint32_t flags,uint32_t count,
    bool rects,bool color,bool depth,bool stencil);
extern "C" int32_t a2fo_backend_clear_dispatch_workflow(void *context,
    bool whole,uint32_t flags,A2foBackendNativeStep native_step,bool *threw);
using A2foBackendEach=bool (*)(void *context,uint32_t index);
extern "C" bool a2fo_backend_each(void *context,uint32_t count,A2foBackendEach callback);
extern "C" int32_t a2fo_backend_copy_admission(void *context,
    const uint32_t *source_format,const uint32_t *destination_format,
    uint32_t count,bool rects,int32_t invalid_call,
    A2foBackendNativeStep native_step,bool *threw);
using A2foBackendCopyStep=void (*)(void *context,uint32_t step,
    uint32_t index,int32_t *result,bool *threw);
extern "C" int32_t a2fo_backend_copy_dispatch(void *context,
    uint32_t count,A2foBackendCopyStep step,bool *threw);
using A2foObserverFactoryStep=void (*)(void *context,uint32_t step,
                                      uintptr_t *result,bool *threw);
extern "C" uintptr_t a2fo_observer_factory_workflow(void *context,
    bool game_test,bool game_ready,A2foObserverFactoryStep step,bool *threw);
extern "C" int32_t a2fo_backend_query_memory_workflow(void *context,
    bool device_present,int32_t unexpected,A2foBackendNativeStep native_step,
    bool *threw);
extern "C" bool a2fo_backend_clear_active(A2foBackendRect bounds,uint32_t flags);
extern "C" uint32_t a2fo_backend_texture_bind_flags(uint32_t format,
    uint32_t shader_resource,uint32_t render_target);
struct A2foObserverActivation {
  uint32_t installed,fo_installed,files,ready,result;
};
extern "C" A2foObserverActivation a2fo_observer_activation_workflow(
    void *context,bool game_test,bool bc_assets,bool fleet_present,
    A2foObserverBridge patch,A2foObserverBridge bridge);
extern "C" uint32_t a2fo_observer_max_events(uint32_t configured);
extern "C" bool a2fo_observer_should_install(bool trace,bool probe,bool game_test);
