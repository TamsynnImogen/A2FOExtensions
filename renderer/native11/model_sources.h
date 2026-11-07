#pragma once
#include "assets.h"
#ifdef __cplusplus
extern "C" {
#endif
// Source identity for each emitted part. Storage is borrowed for the duration
// of the synchronous callback; callers copy any fields they retain.
typedef struct A2FO_N11_PartSource {
    const uint8_t* node_name;
    uint32_t node_name_length;
    uint32_t node_index;
    uint32_t group_index;
} A2FO_N11_PartSource;
typedef int (*A2FO_N11_DetailedPartSink)(void*,const A2FO_N11_Part*,const A2FO_N11_PartSource*);
// Full authored part scope, with stable node/group ordinals. Borrowed callback
// storage expires on return; no native engine state is inspected.
int a2fo_native11_inspect_sod_all(const uint8_t*,uint32_t,A2FO_N11_DetailedPartSink,void*);
#ifdef __cplusplus
}
#endif
