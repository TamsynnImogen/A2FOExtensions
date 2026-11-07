#include "../renderer/native11/model_pose.h"
#include <stddef.h>
_Static_assert(sizeof(A2FO_N11_PoseNode)==72,"x86 node ABI");
_Static_assert(offsetof(A2FO_N11_PoseNode,name)==16,"node name offset");
_Static_assert(offsetof(A2FO_N11_PoseNode,local_matrix34)==24,"node matrix offset");
_Static_assert(sizeof(A2FO_N11_PoseChannel)==28,"x86 channel ABI");
_Static_assert(offsetof(A2FO_N11_PoseChannel,keys)==20,"channel key offset");
_Static_assert(sizeof(A2FO_N11_PoseMetadata)==20,"x86 metadata ABI");
int native11_pose_c_layout(const A2FO_N11_PoseMetadata* p) {
    if(!p||p->node_count!=1||p->channel_count!=2||p->reference_count||
       !p->nodes||!p->channels)return 0;
    return p->nodes[0].index==0&&p->nodes[0].parent==UINT32_MAX&&
        p->nodes[0].kind==1&&p->nodes[0].group_count==1&&
        p->nodes[0].name_length==4&&p->nodes[0].name[0]=='m'&&
        p->nodes[0].local_matrix34[0]==1&&
        p->channels[0].node_index==0&&p->channels[0].key_width==12&&
        p->channels[0].key_count==2&&p->channels[0].keys[12+9]==.25f&&
        p->channels[1].channel_type==5&&p->channels[1].key_width==1&&
        p->channels[1].keys[1]==.5f;
}
