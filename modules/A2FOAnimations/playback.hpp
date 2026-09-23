#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace a2fo::animations {
inline constexpr std::size_t max_clips = 32;
inline constexpr std::array<const char*, 16> events{{
    "spawn", "load", "production_begin", "production_complete", "production_cancel",
    "launch_begin", "launch_clear", "launch_abort", "repair_begin", "repair_end",
    "production", "repair", "move", "move_impulse", "move_warp", "attack"}};
// Highest priority first when several persistent states are active.
inline constexpr std::array<const char*, 6> paired_events{{
    "repair", "production", "attack", "move_warp", "move_impulse", "move"}};
inline bool builtin(const std::string& event) {
    return std::any_of(events.begin(),events.end(),[&](auto name){return event==name;});
}
inline int paired_index(const std::string& event) {
    for(unsigned i=0;i<paired_events.size();++i) if(event==paired_events[i]) return int(i);
    return -1;
}
inline std::string event_key(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(value.size()>4 && value.compare(value.size()-4,4,".odf")==0) value.resize(value.size()-4);
    return value;
}
inline bool valid_event(const std::string& value) {
    return !value.empty() && value.size()<=255 &&
        std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-';});
}
struct Clip {
    std::string event;
    std::uint32_t start = 0, end = 0;
    bool forward = true, repeat = false, reset = false;
    double speed = 1;
};
using Fields = std::map<std::string, std::string>;
inline std::string clean(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    if(a==std::string::npos) return {};
    s=s.substr(a,b-a+1);
    if(s.size()>=2 && s.front()=='"' && s.back()=='"') s=s.substr(1,s.size()-2);
    return s;
}
inline bool number(const std::string& s, double& out) {
    if(s.empty()) return false;
    char* end=nullptr; out=std::strtod(s.c_str(), &end);
    return end==s.c_str()+s.size() && std::isfinite(out);
}
inline bool parse(const Fields& fields, std::vector<Clip>& result, std::string& error) {
    result.clear(); error.clear();
    bool gap=false;
    std::vector<Clip> parsed;
    for(std::size_t i=0;i<=max_clips;++i) {
        const std::string prefix="animation"+std::to_string(i);
        const auto get=[&](const char* suffix)->const std::string* {
            auto f=fields.find(prefix+suffix); return f==fields.end()?nullptr:&f->second;
        };
        const std::array<const char*,7> suffixes{{"event","start","end","direction","repeat","resetonend","speed"}};
        bool any=false; for(auto s:suffixes) any |= get(s)!=nullptr;
        if(!any) {gap=true;continue;}
        const auto fail=[&](const char* why){error=prefix+": "+why;return false;};
        if(i==max_clips) return fail("at most 32 clips are supported");
        if(gap) return fail("indices must be contiguous from 0");
        if(!get("event") || !get("start") || !get("end")) return fail("event, start and end are required");
        Clip c; c.event=event_key(clean(*get("event")));
        if(!valid_event(c.event)) return fail("event must be a builtin name or weapon ODF basename");
        for(const auto& previous:parsed) if(previous.event==c.event) return fail("duplicate event binding");
        double start=0,end=0;
        if(!number(clean(*get("start")),start)||!number(clean(*get("end")),end)||
            start<0 || end<start || end>65534 || std::floor(start)!=start || std::floor(end)!=end)
            return fail("invalid integral frame range (0..65534)");
        c.start=static_cast<std::uint32_t>(start); c.end=static_cast<std::uint32_t>(end);
        auto flag=[&](const char* key,bool& value){
            if(!get(key)) return true;
            auto text=clean(*get(key)); if(text!="0" && text!="1") return false;
            value=text=="1"; return true;
        };
        if(!flag("direction",c.forward)||!flag("repeat",c.repeat)||!flag("resetonend",c.reset))
            return fail("boolean settings must be 0 or 1");
        if(get("speed") && (!number(clean(*get("speed")),c.speed)||c.speed<=0 || c.speed>10000))
            return fail("speed must be finite and in (0,10000]");
        parsed.push_back(c);
    }
    result=std::move(parsed); return true;
}
struct Playback {
    Clip clip{};
    double frame=0, spacing=0, travelled=0;
    bool active=false, finished=false;
    void start(const Clip& value,double seconds_per_sample) {
        clip=value;spacing=seconds_per_sample;travelled=0;active=true;finished=false;
        frame=clip.forward?clip.start:clip.end;
    }
    void redirect(bool forward) {
        clip.forward=forward;clip.repeat=false;clip.reset=false;finished=false;
        travelled=forward?frame-clip.start:clip.end-frame;
    }
    void advance(double seconds) {
        if(!active || finished || !std::isfinite(seconds) || seconds<=0 || spacing<=0) return;
        const double range=clip.end-clip.start;
        if(range==0) {frame=clip.start;finished=!clip.repeat;return;}
        const double next=travelled+seconds*clip.speed/spacing;
        if(!std::isfinite(next)) return;
        travelled=next;
        // A repeating clip includes a full dwell on its terminal sample.
        // Otherwise the forward end frame would never be displayed.
        if(clip.repeat) travelled=std::fmod(travelled,range+1);
        else if(travelled>=range) {
            frame=clip.reset?clip.start:(clip.forward?clip.end:clip.start);
            finished=true;return;
        }
        const double offset=std::min(travelled,range);
        frame=clip.forward?clip.start+offset:clip.end-offset;
    }
    std::uint32_t sample() const {
        // Match native sample-and-hold. Snap only roundoff at exact boundaries.
        return static_cast<std::uint32_t>(std::clamp(std::floor(frame+1e-6),
            double(clip.start),double(clip.end)));
    }
};
} // namespace a2fo::animations
