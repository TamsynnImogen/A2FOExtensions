#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRecoveryConfig {
    uint32_t enabled, engine_context, node_transform, capture_first_match;
    uint32_t sample_every, max_samples, max_reports;
} A2foRsRecoveryConfig;
typedef struct A2foRsRadarSeen { const void* object_class; int32_t pass; } A2foRsRadarSeen;
uint8_t a2fo_rs_radar_api_valid(uint8_t present, uint32_t version, uint32_t expected_version,
    size_t size, size_t minimum, uint8_t has_log, uint8_t has_image,
    uint8_t has_patch, uint8_t has_root);
uint8_t a2fo_rs_radar_mark_seen(A2foRsRadarSeen* entries, uint32_t* count,
    const void* object_class, int32_t pass);
uint8_t a2fo_rs_recovery_parse_number(const uint8_t* text, size_t length,
    uint32_t minimum, uint32_t maximum, uint32_t* out);
uint8_t a2fo_rs_recovery_config_valid(const A2foRsRecoveryConfig* config);
typedef uint64_t (*A2foRsRuntimeBOp)(void* context, uint32_t operation);
uint8_t a2fo_rs_radar_init(uint8_t valid, uint8_t installed, void* context, A2foRsRuntimeBOp op);
uint8_t a2fo_rs_radar_install(void* context, A2foRsRuntimeBOp op);
void a2fo_rs_radar_object(uint64_t activation, void* context, A2foRsRuntimeBOp op);
uint32_t a2fo_rs_radar_clear(uint64_t activation, uint8_t scope, void* context, A2foRsRuntimeBOp op);
uint8_t a2fo_rs_recovery_init(uint8_t valid, void* context, A2foRsRuntimeBOp op);
void a2fo_rs_recovery_before(uint32_t kind, uint64_t activation, void* context, A2foRsRuntimeBOp op);
void a2fo_rs_recovery_after(uint32_t admitted, uint64_t activation, void* context, A2foRsRuntimeBOp op);
uint64_t a2fo_rs_radar_activation(uint8_t capturing, uint8_t installed, void* context, A2foRsRuntimeBOp op);
uint64_t a2fo_rs_recovery_activation(uint8_t closing, void* context, A2foRsRuntimeBOp op);
void a2fo_rs_radar_shutdown(void* context, A2foRsRuntimeBOp op);
void a2fo_rs_recovery_shutdown(void* context, A2foRsRuntimeBOp op);
void a2fo_rs_recovery_status(uint64_t activation, void* context, A2foRsRuntimeBOp op);
#ifdef __cplusplus
}
#endif
