#include "light_event.hpp"
#include <string_view>
namespace a2fo::native11 {
namespace {
bool known(A2FO_StringView v) noexcept {
    if(!v.data||!v.size||v.size>64)return false;
    for(const char* name:requested_light_fields()){
        const std::string_view expected(name);
        if(expected.size()!=v.size)continue;
        bool equal=true;
        for(std::size_t i=0;i<v.size;++i){char c=v.data[i],d=name[i];
            if(c>='A'&&c<='Z')c=char(c-'A'+'a');
            if(d>='A'&&d<='Z')d=char(d-'A'+'a');
            if(c!=d){equal=false;break;}}
        if(equal)return true;
    }
    return false;
}
}
bool copy_light_config(const A2FO_GameObjectClassLoadedEvent& event,LightConfig& output,
                       std::string& error) noexcept {
    try {
        if(event.struct_size<sizeof(event)||!event.object_class||event.odf_field_count>128||
           (event.odf_field_count&&!event.odf_fields)){
            error="invalid copied class-ODF light event";return false;
        }
        std::vector<LightField> fields;fields.reserve(10);
        for(std::uint32_t i=0;i<event.odf_field_count;++i){const auto& f=event.odf_fields[i];
            if(!known(f.name))continue;
            if(!f.value.data||f.value.size>256){error="invalid copied light field value";return false;}
            fields.push_back({std::string(f.name.data,f.name.size),std::string(f.value.data,f.value.size)});
        }
        return parse_light_config(fields,output,error);
    }catch(...){try{error="copied light configuration allocation failure";}catch(...){}return false;}
}
}
