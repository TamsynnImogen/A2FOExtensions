#include "../renderer/native11/light_class_store.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace a2fo::native11;
namespace {
void need(bool ok,const char* name) {
    if (!ok) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); }
}
A2FO_GameObjectClassLoadedEvent event(std::uintptr_t id,const char* path,
    const A2FO_OdfFieldView* fields,std::uint32_t count) {
    return {sizeof(A2FO_GameObjectClassLoadedEvent),reinterpret_cast<void*>(id),
        {path,static_cast<std::uint32_t>(std::strlen(path))},fields,count};
}
}
int main() {
    LightClassStore store;std::string error;
    char path[]="sun.odf",source[]="1",type[]="directional",colour[]="1 0.5 0.25";
    A2FO_OdfFieldView fields[]={
        {{"lightSource",11},{source,1}},
        {{"lightType",9},{type,11}},
        {{"lightColor",10},{colour,10}}};
    auto e=event(10,path,fields,3);
    need(store.update(e,error)&&store.healthy()&&store.size()==1&&error.empty(),"first load");
    auto first=store.find(10);
    need(first&&first->config.enabled&&first->source_odf=="sun.odf"&&first->version>0,
        "owned enabled record");
    const auto first_version=first->version;
    std::memset(path,'x',7);std::memset(colour,'x',10);
    need(first->source_odf=="sun.odf"&&first->config.light.colour[1]==0.5f,
        "callback storage not retained");
    char fresh_colour[]="1 0.5 0.25";
    fields[2].value={fresh_colour,10};
    auto other=event(20,"other.odf",fields,3);
    need(store.update(other,error)&&store.find(20)&&store.healthy(),"second class load");
    char bad_source[]="2";
    fields[0].value={bad_source,1};
    need(!store.update(e,error)&&!store.healthy()&&store.size()==2&&store.find(20)&&
         !store.find(10)&&!error.empty(),"malformed reload faults class");
    need(first->config.enabled&&first->version==first_version,
        "retained old snapshot survives fault");
    char disabled[]="0";
    fields[0].value={disabled,1};
    e=event(10,"dark.odf",fields,3);
    need(store.update(e,error)&&store.healthy()&&store.size()==2&&
         store.find(10)&&!store.find(10)->config.enabled&&
         store.find(10)->version>first_version,"successful same-class recovery and disabled record");
    auto second=store.find(10);
    need(first->source_odf=="sun.odf"&&second->source_odf=="dark.odf",
        "old and replacement owners independent");
    e=event(10,"other.odf",fields,3);
    need(store.update(e,error)&&store.find(10)->version>second->version&&
         second->source_odf=="dark.odf","replacement advances version");
    auto bad_event=e;bad_event.object_class=nullptr;
    need(!store.update(bad_event,error)&&!store.healthy()&&!store.find(10),
        "null class latches global fault");
    need(store.update(e,error)&&!store.healthy()&&!store.find(10),
        "global fault requires clear");
    const auto before_clear=second->version;
    store.clear();
    need(store.healthy()&&store.size()==0&&!store.find(10),"clear resets fault and records");
    need(store.update(e,error)&&store.find(10)->version>before_clear,
        "clear never reuses version");
    store.clear();
    bad_event=e;bad_event.odf_field_count=129;
    need(!store.update(bad_event,error)&&!store.healthy(),"field overflow global fault");
    store.clear();
    std::string long_path(260,'p');
    bad_event=event(10,long_path.c_str(),fields,3);
    need(!store.update(bad_event,error)&&!store.healthy(),"ODF path cap global fault");
    store.clear();
    std::string maximum_path(259,'p');
    e=event(10,maximum_path.c_str(),fields,3);
    need(store.update(e,error)&&store.find(10)->source_odf.size()==259,"ODF path max accepted");
    store.clear();
    for (std::uintptr_t id=1;id<=4096;++id) {
        e=event(id,"a.odf",fields,3);
        need(store.update(e,error),"4096 class records accepted");
    }
    need(store.healthy()&&store.size()==4096,"capacity exact");
    e=event(4097,"a.odf",fields,3);
    need(!store.update(e,error)&&!store.healthy()&&store.size()==4096&&
         !store.find(1),"capacity overflow latches global fault");
    store.clear();
    need(store.healthy()&&store.size()==0,"clear recovers capacity fault");
    std::puts("native11 light class store: PASS");
}
