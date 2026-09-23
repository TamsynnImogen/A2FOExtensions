#include "../modules/A2FOAnimations/playback.hpp"
#include <iostream>
#include <stdexcept>
using namespace a2fo::animations;
#define CHECK(x) do {if(!(x)) throw std::runtime_error(#x);}while(0)
int main() {
    std::vector<Clip> clips;std::string error;
    Fields f{{"animation0event","\"repair_begin\""},{"animation0start","10"},{"animation0end","20"}};
    CHECK(parse(f,clips,error));CHECK(clips.size()==1);CHECK(clips[0].forward && !clips[0].repeat && !clips[0].reset && clips[0].speed==1);
    Playback p;p.start(clips[0],0.1);p.advance(0.5);CHECK(p.sample()==15);p.advance(0.5);CHECK(p.sample()==20 && p.finished);p.advance(50);CHECK(p.sample()==20);
    auto c=clips[0];c.forward=false;p.start(c,0.1);CHECK(p.sample()==20);p.advance(0.5);CHECK(p.sample()==15);p.advance(0.5);CHECK(p.sample()==10 && p.finished);
    c.forward=true;c.reset=true;p.start(c,0.1);p.advance(1);CHECK(p.sample()==10 && p.finished);
    c.repeat=true;p.start(c,0.1);p.advance(0.5);CHECK(p.sample()==15 && !p.finished);p.advance(0.5);CHECK(p.sample()==20 && !p.finished);p.advance(0.1);CHECK(p.sample()==10);
    c.forward=false;p.start(c,0.1);p.advance(0.5);CHECK(p.sample()==15);p.advance(0.5);CHECK(p.sample()==10);p.advance(0.1);CHECK(p.sample()==20);
    c.repeat=false;c.reset=false;c.speed=2;p.start(c,0.1);p.advance(0.25);CHECK(p.sample()==15);p.advance(0.25);CHECK(p.sample()==10 && p.finished);
    c.start=c.end=12;c.repeat=true;p.start(c,0.1);p.advance(1e9);CHECK(p.sample()==12 && !p.finished);
    c=clips[0];Playback a,b;a.start(c,0.1);c.forward=false;b.start(c,0.1);a.advance(0.2);b.advance(0.3);CHECK(a.sample()==12 && b.sample()==17);
    a.advance(-5);a.advance(INFINITY);a.advance(NAN);CHECK(a.sample()==12);
    b.start(clips[0],0.1);CHECK(b.sample()==10 && !b.finished);
    for(auto bad:{"-1","0.5","nan","inf","65535"}) {auto broken=f;broken["animation0end"]=bad;CHECK(!parse(broken,clips,error));CHECK(clips.empty());}
    for(auto bad:{"0","-1","nan","inf","1abc"}) {auto broken=f;broken["animation0speed"]=bad;CHECK(!parse(broken,clips,error));}
    for(auto key:{"direction","repeat","resetonend"}) {auto broken=f;broken[std::string("animation0")+key]="2";CHECK(!parse(broken,clips,error));}
    auto broken=f;broken["animation0event"]="bad/event!";CHECK(!parse(broken,clips,error));
    broken=f;broken["animation2repeat"]="1";CHECK(!parse(broken,clips,error));
    broken=f;broken.erase("animation0start");CHECK(!parse(broken,clips,error));
    broken=f;broken["animation1event"]="repair_begin";broken["animation1start"]="0";broken["animation1end"]="1";CHECK(!parse(broken,clips,error));
    Fields full;for(unsigned i=0;i<events.size();++i) {auto prefix="animation"+std::to_string(i);full[prefix+"event"]=events[i];full[prefix+"start"]="0";full[prefix+"end"]="100";}
    CHECK(parse(full,clips,error) && clips.size()==events.size());
    CHECK(parse({},clips,error) && clips.empty());
    f["animation0event"]="FPhoton.ODF";CHECK(parse(f,clips,error));CHECK(clips[0].event=="fphoton");
    CHECK(builtin("production") && !builtin("fphoton"));CHECK(paired_index("attack")==2);
    // Reverse in place on interruption; opening again preserves the cursor.
    c={"production",0,10};p.start(c,0.1);p.advance(0.4);CHECK(p.sample()==4);
    p.redirect(false);CHECK(p.sample()==4);p.advance(0.2);CHECK(p.sample()==2);
    p.redirect(true);CHECK(p.sample()==2);p.advance(0.8);CHECK(p.sample()==10 && p.finished);
    p.redirect(false);p.advance(1);CHECK(p.sample()==0 && p.finished);

    std::cout<<"Animation policy: parser, defaults, ranges, forward/reverse, loops, reset/hold, speed, interruption and independent instances passed\n";
}
