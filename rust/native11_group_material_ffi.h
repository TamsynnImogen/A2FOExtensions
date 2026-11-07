#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// The sink borrows the material name for this synchronous call only.
typedef struct A2FO_N11_GroupMaterial {
    uint32_t node_index,group_index;
    const uint8_t* material_name;
    uint32_t material_name_length,model,self_illuminating;
    float diffuse[3],ambient[3],specular[3],power;
} A2FO_N11_GroupMaterial;
typedef int32_t (*A2FO_N11_GroupMaterialSink)(void*,const A2FO_N11_GroupMaterial*);
int32_t a2fo_rs_native11_inspect_sod_group_material(
    const uint8_t*,uint32_t,uint32_t,uint32_t,A2FO_N11_GroupMaterialSink,void*);
#ifdef __cplusplus
}
#endif
