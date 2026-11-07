#include "../renderer/native11/light_config.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool pass,const char* name) {
    if (!pass) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); }
}
void rejected(std::vector<LightField> fields,const char* name) {
    LightConfig config{};
    config.enabled=true;config.light.intensity=17;config.cast_shadows=false;
    config.legacy_native_source=true;
    std::string error="before";
    need(!parse_light_config(fields,config,error),name);
    need(config.enabled&&config.light.intensity==17&&!config.cast_shadows&&
         config.legacy_native_source,"failure is transactional");
    need(!error.empty()&&error!="before","failure diagnostic");
}
std::vector<LightField> base() {
    return {{"lightSource","1"},{"lightType","point"},{"lightColor","0.5 1 2"}};
}
}
int main() {
    const auto& requested=requested_light_fields();
    need(requested.size()==20&&std::string(requested[0])=="lightSource"&&
         std::string(requested[9])=="lightCastShadows"&&
         std::string(requested[10])=="a2foLightSource"&&
         std::string(requested[19])=="a2foLightCastShadows","requested fields");
    LightConfig config{};config.enabled=true;
    std::string error="old";
    need(parse_light_config({},config,error)&&!config.enabled&&
         !config.legacy_native_source&&error.empty(),"absent is disabled");
    const std::vector<LightField> disabled{{"lightSource","0"},{"lightType","garbage"},
        {"lightColor","NaN"},{"lightColor","duplicate"}};
    need(parse_light_config(disabled,config,error)&&!config.enabled&&
         !config.legacy_native_source,
        "disabled ignores other controls");
    auto fields=base();
    fields[0].name="LIGHTsource";fields[1].value="\"SpOt\"";
    fields.push_back({"lightDirection","3\t4 0"});
    fields.push_back({"lightIntensity","2"});
    fields.push_back({"lightFalloffStart","12"});
    fields.push_back({"lightFalloffRange","20"});
    fields.push_back({"lightConeInner","20"});
    fields.push_back({"lightConeOuter","40"});
    fields.push_back({"lightCastShadows","0"});
    need(parse_light_config(fields,config,error)&&config.enabled&&
         config.legacy_native_source&&error.empty(),"spot parses");
    need(config.light.type==LightType::spot&&config.light.direction[0]==0.6f&&
         config.light.direction[1]==0.8f&&config.light.direction[2]==0,
         "local direction normalized");
    need(config.light.colour[0]==0.5f&&config.light.intensity==2&&
         config.light.falloff_start==12&&config.light.falloff_range==20&&
         config.light.cone_inner_degrees==20&&config.light.cone_outer_degrees==40&&
         !config.cast_shadows,"spot values");
    fields.clear();
    need(config.light.direction[0]==0.6f&&config.light.colour[2]==2,"input lifetime owned");
    auto point=base();
    need(parse_light_config(point,config,error)&&config.light.type==LightType::point&&
         config.light.falloff_start==100&&config.light.falloff_range==5&&
         config.light.intensity==1&&config.cast_shadows&&config.legacy_native_source,
         "FO baseline defaults and native source identity");
    point[1].value="directional";
    need(parse_light_config(point,config,error)&&config.light.direction[1]==-1,
         "directional default direction");
    std::vector<LightField> prefixed{{"a2foLightSource","1"},
        {"a2foLightType","\"SpOt\""},{"a2foLightColor","0.5 1 2"},
        {"a2foLightDirection","3\t4 0"},{"a2foLightIntensity","2"},
        {"a2foLightFalloffStart","12"},{"a2foLightFalloffRange","20"},
        {"a2foLightConeInner","20"},{"a2foLightConeOuter","40"},
        {"a2foLightCastShadows","0"},{"lightSource","0"}};
    need(parse_light_config(prefixed,config,error)&&config.enabled&&
         !config.legacy_native_source&&config.light.type==LightType::spot&&
         config.light.direction[0]==0.6f&&config.light.direction[1]==0.8f&&
         config.light.colour[0]==0.5f&&config.light.colour[2]==2&&
         config.light.intensity==2&&config.light.falloff_start==12&&
         config.light.falloff_range==20&&config.light.cone_inner_degrees==20&&
         config.light.cone_outer_degrees==40&&!config.cast_shadows,
         "all prefixed aliases preserve parser values without native identity");
    auto no_legacy_source=prefixed;no_legacy_source.pop_back();
    rejected(no_legacy_source,"enabled prefixed light requires explicit legacy zero");
    auto mixed=prefixed;
    mixed.push_back({"lightType","directional"});
    mixed.push_back({"lightFalloffRange","NaN"});
    need(parse_light_config(mixed,config,error)&&config.enabled&&
         !config.legacy_native_source&&config.light.type==LightType::spot&&
         config.light.colour[0]==0.5f,"legacy type and falloff do not fill prefixed family");
    auto mixed_colour=mixed;
    mixed_colour.push_back({"lightColor","1 0 0"});
    rejected(mixed_colour,"legacy lightColor creates native point even with source zero");
    mixed_colour.back().name="LIGHTCOLOR";
    rejected(mixed_colour,"casefold legacy lightColor also refused");
    mixed_colour.push_back({"lightColor","duplicate"});
    rejected(mixed_colour,"duplicate legacy lightColor cannot be ignored");
    const auto legacy_index=prefixed.size()-1;
    mixed[legacy_index].value="1";rejected(mixed,"legacy native source cannot coexist");
    mixed[legacy_index].value="invalid";rejected(mixed,"malformed legacy source cannot coexist");
    mixed[legacy_index].value="0";
    mixed.push_back({"LIGHTSOURCE","0"});rejected(mixed,"duplicate legacy source with prefix");
    rejected({{"a2foLightColor","1 1 1"}},"partial prefixed family requires source");
    rejected({{"lightSource","1"},{"lightType","point"},{"lightColor","1 1 1"},
              {"a2foLightIntensity","1"}},"old family cannot fill partial prefixed family");
    auto prefixed_duplicate=prefixed;
    prefixed_duplicate.push_back({"A2FOLIGHTCOLOR","1 1 1"});
    rejected(prefixed_duplicate,"duplicate prefixed known key casefold");
    prefixed_duplicate=prefixed;prefixed_duplicate.push_back({"a2foLightSource","0"});
    rejected(prefixed_duplicate,"duplicate prefixed source");
    prefixed_duplicate={{"a2foLightSource","0"},{"a2foLightType","point"},
                        {"A2FOLIGHTTYPE","spot"}};
    rejected(prefixed_duplicate,"disabled prefixed family still rejects duplicate known keys");
    rejected({{"a2foLightSource","2"}},"strict prefixed source boolean");
    auto prefixed_bad=prefixed;prefixed_bad[2].value="NaN";
    rejected(prefixed_bad,"prefixed malformed colour");
    prefixed_bad=prefixed;prefixed_bad[3].value="-1";
    rejected(prefixed_bad,"prefixed negative intensity");
    prefixed_bad=prefixed;prefixed_bad.push_back({"unknownPrefixedField","garbage"});
    need(parse_light_config(prefixed_bad,config,error)&&config.enabled&&
         !config.legacy_native_source,"unknown prefixed field ignored");
    const std::vector<LightField> disabled_prefixed{{"a2foLightSource","0"},
        {"a2foLightType","garbage"},{"lightColor","1 0 0"},
        {"LIGHTCOLOR","duplicate legacy field"}};
    need(parse_light_config(disabled_prefixed,config,error)&&
         !config.enabled&&!config.legacy_native_source,
         "disabled prefixed source does not create light");
    rejected({{"lightSource","1"}},"required fields");
    rejected({{"lightSource","2"}},"source boolean");
    rejected({{"lightSource","1"},{"LIGHTSOURCE","0"}},"duplicate source casefold");
    rejected({{"lightSource","1"},{"lightType","point"},{"LIGHTTYPE","spot"},
        {"lightColor","1 1 1"}},"duplicate known field casefold");
    const char* bad_numbers[]={"NaN","Inf","1e999","1junk","0x1p0","1,2,3","", "1 2", "1 2 3 4"};
    for (const char* bad:bad_numbers) {
        auto f=base();f[2].value=bad;rejected(f,"malformed colour vector");
    }
    auto f=base();f[2].value="-1 0 0";rejected(f,"negative colour");
    f=base();f.push_back({"lightIntensity","-1"});rejected(f,"negative intensity");
    f=base();f.push_back({"lightIntensity","1e39"});rejected(f,"intensity float overflow");
    f=base();f[2].value="3e38 1 1";f.push_back({"lightIntensity","2"});
    rejected(f,"colour times intensity overflow");
    f=base();f[1].value="spot";f.push_back({"lightDirection","0 0 0"});
    rejected(f,"zero direction");
    f=base();f[1].value="beam";rejected(f,"unknown type");
    f=base();f.push_back({"lightFalloffRange","0"});rejected(f,"zero local range");
    f=base();f.push_back({"lightFalloffStart","-2"});rejected(f,"negative local start");
    f=base();f[1].value="spot";f.push_back({"lightConeInner","61"});
    f.push_back({"lightConeOuter","60"});rejected(f,"inverted cone");
    f.back().value="180";rejected(f,"outer 180");
    f=base();f.push_back({"lightCastShadows","true"});rejected(f,"strict shadow boolean");
    f=base();f.push_back({"lightConeInner","NaN"});rejected(f,"invalid explicit known field");
    f=base();f.push_back({"lightConeOuter","180"});rejected(f,"invalid irrelevant explicit cone");
    f=base();f.push_back({"irrelevantUnknown","junk"});
    need(parse_light_config(f,config,error)&&config.enabled,"unknown field ignored");
    f=base();f[0].name=std::string(65,'x');rejected(f,"name cap");
    f=base();f[2].value=std::string(257,'1');rejected(f,"value cap");
    f=base();while (f.size()<=128) f.push_back({"unknown","x"});rejected(f,"field count cap");
    std::puts("native11 light config: PASS");
}
