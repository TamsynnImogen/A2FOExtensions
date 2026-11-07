#include "../renderer/native11/light_event.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace a2fo::native11;
static void need(bool x,const char* why){if(!x){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
    char source[]="1",type[]="\"spot\"",colour[]=".2 .4 .8",direction[]="0 0 2";
    A2FO_OdfFieldView fields[]={{{"lightSource",11},{source,1}},{{"lightType",9},{type,6}},
        {{"lightColor",10},{colour,8}},{{"lightDirection",14},{direction,5}}};
    A2FO_GameObjectClassLoadedEvent event{sizeof(event),reinterpret_cast<void*>(1),{nullptr,0},fields,4};
    LightConfig output;std::string error;
    need(copy_light_config(event,output,error)&&output.enabled&&output.light.type==LightType::spot,"copied event config");
    need(output.light.colour[2]==.8f&&output.light.direction[2]==1,"copied values normalized");
    std::memset(colour,'x',sizeof(colour));std::memset(type,'x',sizeof(type));
    need(output.light.colour[2]==.8f&&output.light.type==LightType::spot,"event lifetime independent");
    event.odf_field_count=129;
    need(!copy_light_config(event,output,error)&&output.enabled,"oversized event transactional");
    event.odf_field_count=4;event.odf_fields=nullptr;
    need(!copy_light_config(event,output,error),"null field view refused");
    event.odf_field_count=0;event.object_class=nullptr;
    need(!copy_light_config(event,output,error),"missing class refused");
    event.object_class=reinterpret_cast<void*>(1);
    need(copy_light_config(event,output,error)&&!output.enabled,"no fields no implicit light");
    std::puts("PASS native11 copied ODF light event");
}
