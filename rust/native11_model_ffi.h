#pragma once
#include "../renderer/native11/assets.h"
#include "../renderer/native11/model_sources.h"
#include "../renderer/native11/model_pose.h"
#ifdef __cplusplus
extern "C" {
#endif
// Private no_std core adapter to the shared pristine static SOD policy.
// Synchronous borrowed inputs/sink; caller copies all required metadata.
// Core CRT allocation and panic-abort policy are unchanged.
int a2fo_rs_native11_parse_sod(const uint8_t*,uint32_t,A2FO_N11_PartSink,void*);
int a2fo_rs_native11_inspect_sod(const uint8_t*,uint32_t,A2FO_N11_DetailedPartSink,void*);
int a2fo_rs_native11_inspect_sod_all(const uint8_t*,uint32_t,A2FO_N11_DetailedPartSink,void*);
int a2fo_rs_native11_inspect_sod_pose(const uint8_t*,uint32_t,A2FO_N11_PoseSink,void*);
#ifdef __cplusplus
}
#endif
