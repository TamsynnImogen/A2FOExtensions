#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Private authored SOD pose metadata. All pointers are borrowed only during
// the synchronous callback; the caller must copy data it retains. Matrices
// retain a2fmt's native 12-float basis-axis-then-translation order.
typedef struct A2FO_N11_PoseNode {
    uint32_t index, parent, kind, group_count;
    const uint8_t* name;
    uint32_t name_length;
    float local_matrix34[12];
} A2FO_N11_PoseNode;
// parent == UINT32_MAX identifies a root. kind: 0 null, 1 mesh, 2 light,
// 3 sprite, 12 emitter. a2fmt maps unknown stored node types to null.
typedef struct A2FO_N11_PoseChannel {
    uint32_t index, node_index, channel_type, key_count;
    float period;
    const float* keys;
    uint32_t key_width; // 12 matrix floats or 1 scale float per key
} A2FO_N11_PoseChannel;
typedef struct A2FO_N11_PoseMetadata {
    uint32_t node_count, channel_count, reference_count;
    const A2FO_N11_PoseNode* nodes;
    const A2FO_N11_PoseChannel* channels;
} A2FO_N11_PoseMetadata;
typedef int (*A2FO_N11_PoseSink)(void*,const A2FO_N11_PoseMetadata*);
#ifdef __cplusplus
}
#endif
