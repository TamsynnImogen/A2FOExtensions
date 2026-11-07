#include "light_config.hpp"
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string_view>

namespace a2fo::native11 {
namespace {
constexpr std::array<const char*,20> names{{
    "lightSource","lightType","lightColor","lightIntensity","lightDirection",
    "lightFalloffStart","lightFalloffRange","lightConeInner","lightConeOuter","lightCastShadows",
    "a2foLightSource","a2foLightType","a2foLightColor","a2foLightIntensity",
    "a2foLightDirection","a2foLightFalloffStart","a2foLightFalloffRange",
    "a2foLightConeInner","a2foLightConeOuter","a2foLightCastShadows"}};

bool space(char c) { return c==' ' || c=='\t'; }
std::string_view trim(std::string_view s) {
    while (!s.empty() && space(s.front())) s.remove_prefix(1);
    while (!s.empty() && space(s.back())) s.remove_suffix(1);
    return s;
}
char lower(char c) { return c>='A'&&c<='Z' ? char(c-'A'+'a') : c; }
bool equal_fold(std::string_view a,std::string_view b) {
    if (a.size()!=b.size()) return false;
    for (std::size_t i=0;i<a.size();++i) if (lower(a[i])!=lower(b[i])) return false;
    return true;
}
int known_field(std::string_view name) {
    for (std::size_t i=0;i<names.size();++i) if (equal_fold(name,names[i])) return int(i);
    return -1;
}
bool decimal(std::string_view s,double& result) {
    if (s.empty()) return false;
    std::size_t i=0;
    if (s[i]=='+'||s[i]=='-') ++i;
    std::size_t digits=0;
    while (i<s.size()&&s[i]>='0'&&s[i]<='9') { ++i;++digits; }
    if (i<s.size()&&s[i]=='.') {
        ++i;
        while (i<s.size()&&s[i]>='0'&&s[i]<='9') { ++i;++digits; }
    }
    if (!digits) return false;
    if (i<s.size()&&(s[i]=='e'||s[i]=='E')) {
        ++i;
        if (i<s.size()&&(s[i]=='+'||s[i]=='-')) ++i;
        const std::size_t exponent_start=i;
        while (i<s.size()&&s[i]>='0'&&s[i]<='9') ++i;
        if (i==exponent_start) return false;
    }
    if (i!=s.size()) return false;
    // from_chars is locale-independent. It does not accept a leading plus.
    if (s.front()=='+') s.remove_prefix(1);
    const auto parsed=std::from_chars(s.data(),s.data()+s.size(),result,std::chars_format::general);
    return parsed.ec==std::errc{}&&parsed.ptr==s.data()+s.size()&&std::isfinite(result);
}
bool scalar(std::string_view raw,float& result) {
    double d=0;
    if (!decimal(trim(raw),d)||d>std::numeric_limits<float>::max()||
        d<-std::numeric_limits<float>::max()) return false;
    result=static_cast<float>(d);
    return std::isfinite(result);
}
bool vector3(std::string_view raw,float out[3]) {
    raw=trim(raw);
    for (int i=0;i<3;++i) {
        const std::size_t end=raw.find_first_of(" \t");
        const auto part=raw.substr(0,end);
        if (!scalar(part,out[i])) return false;
        if (i==2) return end==std::string_view::npos;
        if (end==std::string_view::npos) return false;
        raw=trim(raw.substr(end));
    }
    return false;
}
bool boolean(std::string_view raw,bool& value) {
    raw=trim(raw);
    if (raw=="0") { value=false;return true; }
    if (raw=="1") { value=true;return true; }
    return false;
}
bool normalize(float v[3]) {
    const double x=v[0],y=v[1],z=v[2];
    const double length=std::hypot(x,y,z);
    if (!(length>0)||!std::isfinite(length)) return false;
    v[0]=float(x/length);v[1]=float(y/length);v[2]=float(z/length);
    return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
}
} // namespace

const std::array<const char*,20>& requested_light_fields() noexcept { return names; }

bool parse_light_config(const std::vector<LightField>& fields,LightConfig& output,std::string& error) {
    const auto fail=[&](const char* message) { error=message;return false; };
    if (fields.size()>128) return fail("too many light fields (max 128)");
    for (const auto& field:fields)
        if (field.name.size()>64||field.value.size()>256)
            return fail("light field exceeds name/value limit (64/256)");

    const LightField* old_source=nullptr;
    bool prefixed=false;
    for (const auto& field:fields) {
        const int index=known_field(field.name);
        if (index>=10) prefixed=true;
        if (index==0) {
            if (old_source) return fail("duplicate lightSource");
            old_source=&field;
        }
    }
    LightConfig candidate{};
    const LightField* values[10]{};
    if (prefixed) {
        if (old_source) {
            bool old_enabled=false;
            if (!boolean(old_source->value,old_enabled)||old_enabled)
                return fail("prefixed light requires legacy lightSource 0");
        }
        for (const auto& field:fields) {
            const int index=known_field(field.name);
            if (index<10) continue;
            const unsigned selected=static_cast<unsigned>(index-10);
            if (values[selected]) return fail("duplicate prefixed light field");
            values[selected]=&field;
        }
        if (!values[0]) return fail("prefixed light requires a2foLightSource");
    } else {
        values[0]=old_source;
    }
    if (!values[0]) { output=candidate;error.clear();return true; }
    if (!boolean(values[0]->value,candidate.enabled))
        return fail(prefixed?"invalid a2foLightSource (expected 0 or 1)":
                             "invalid lightSource (expected 0 or 1)");
    if (!candidate.enabled) { output=candidate;error.clear();return true; }
    if (prefixed) {
        if (!old_source)
            return fail("enabled prefixed light requires explicit legacy lightSource 0");
        for (const auto& field:fields)
            if (known_field(field.name)==2)
                return fail("enabled prefixed light cannot include legacy lightColor");
    }
    if (!prefixed) {
        for (const auto& field:fields) {
            const int index=known_field(field.name);
            if (index<=0||index>=10) continue;
            if (values[index]) return fail("duplicate light field");
            values[index]=&field;
        }
    }
    if (!values[1]||!values[2]) return fail("enabled light requires lightType and lightColor");
    auto type=trim(values[1]->value);
    if (type.size()>=2&&type.front()=='"'&&type.back()=='"') type=trim(type.substr(1,type.size()-2));
    if (equal_fold(type,"directional")) candidate.light.type=LightType::directional;
    else if (equal_fold(type,"point")) candidate.light.type=LightType::point;
    else if (equal_fold(type,"spot")) candidate.light.type=LightType::spot;
    else return fail("invalid lightType");

    if (!vector3(values[2]->value,candidate.light.colour)) return fail("invalid lightColor");
    for (float component:candidate.light.colour) if (component<0) return fail("negative lightColor");
    if (values[3]&&(!scalar(values[3]->value,candidate.light.intensity)||candidate.light.intensity<0))
        return fail("invalid lightIntensity");
    if (values[4]&&!vector3(values[4]->value,candidate.light.direction)) return fail("invalid lightDirection");
    candidate.light.falloff_start=100;
    candidate.light.falloff_range=5;
    if (values[5]&&(!scalar(values[5]->value,candidate.light.falloff_start)||candidate.light.falloff_start<0))
        return fail("invalid lightFalloffStart");
    if (values[6]&&(!scalar(values[6]->value,candidate.light.falloff_range)||candidate.light.falloff_range<=0))
        return fail("invalid lightFalloffRange");
    if (values[7]&&!scalar(values[7]->value,candidate.light.cone_inner_degrees)) return fail("invalid lightConeInner");
    if (values[8]&&!scalar(values[8]->value,candidate.light.cone_outer_degrees)) return fail("invalid lightConeOuter");
    if (values[9]&&!boolean(values[9]->value,candidate.cast_shadows)) return fail("invalid lightCastShadows");

    if (candidate.light.type!=LightType::point&&!normalize(candidate.light.direction)) return fail("zero lightDirection");
    if (double(candidate.light.falloff_start)+candidate.light.falloff_range>std::numeric_limits<float>::max())
        return fail("light falloff distance overflows");
    if ((candidate.light.type==LightType::spot||values[7]||values[8]) &&
        !(candidate.light.cone_inner_degrees>0&&
          candidate.light.cone_inner_degrees<=candidate.light.cone_outer_degrees&&
          candidate.light.cone_outer_degrees<180)) return fail("invalid light cone angles");
    for (float component:candidate.light.colour)
        if (double(component)*candidate.light.intensity>std::numeric_limits<float>::max())
            return fail("lightColor times lightIntensity overflows");
    candidate.legacy_native_source=!prefixed;
    output=candidate;
    error.clear();
    return true;
}
} // namespace a2fo::native11
