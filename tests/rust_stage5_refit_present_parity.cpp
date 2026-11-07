// Compile separately with -Ibuild/rust-migration-stage5/before and with -I.
// Each binary uses the same public headers and emits exact, comparable values.
#include <modules/A2FORefitMenus/loading_content.hpp>
#include <modules/A2FORefitMenus/setup_presentation.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

using namespace a2fo::refit_menu;
static std::uint32_t bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

static void rect(const Rect& r) {
    std::cout << r.x << ',' << r.y << ',' << r.width << ',' << r.height << ';';
}
static void text(const std::string& value) {
    std::cout << value.size() << ':';
    for (unsigned char c : value) {
        constexpr char hex[] = "0123456789abcdef";
        std::cout << hex[c >> 4] << hex[c & 15];
    }
    std::cout << ';';
}
static void menu() {
    auto snapshot = default_snapshot();
    for (auto [w,h] : {std::pair{0,0}, {80,99}, {640,480}, {800,600},
                       {1280,720}, {1920,1080}, {2560,1440}, {3440,1440}}) {
        const auto l = layout(w,h,snapshot);
        std::cout << "menu " << w << ' ' << h << ' ' << bits(l.scale) << ' ';
        rect(l.title);for (const auto& row:l.rows) rect(row);rect(l.footer);
        for (int i=-2;i<9;++i)std::cout << next_enabled(snapshot,i,1) << ',' << next_enabled(snapshot,i,-1) << ';';
        std::cout << '\n';
    }
    snapshot.entries[1].visible=false;snapshot.entries[3].enabled=false;
    const auto l=layout(1280,720,snapshot);
    for (int y:{-1,0,250,500,719})for (int x:{-1,0,100,400,1279})
        std::cout << "hit " << x << ' ' << y << ' ' << hit_test(l,snapshot,x,y) << '\n';
}
static setup::Model setup_fixture() {
    setup::Model m;std::uintptr_t key=1;
    for(int row=0;row<8;++row)for(int col=0;col<3;++col) {
        setup::Control c;c.key=key++;c.kind=setup::Kind::combo;
        c.source={50+col*125,20+row*28,100,22};c.text="Selected";
        c.items={"One","Two","Three","Four","Five","Six","Seven","Eight","Nine","Ten"};
        c.pull=c.source;c.item_height=22;m.controls.push_back(c);
    }
    for(int i=0;i<8;++i){setup::Control c;c.key=key++;c.kind=setup::Kind::picture;c.source={30,20+i*28,16,16};m.controls.push_back(c);}
    for(int i=0;i<2;++i){setup::Control c;c.key=key++;c.kind=setup::Kind::button;c.source={400+i*140,580,120,28};c.text=i?"Launch":"Back";m.controls.push_back(c);}
    for(int i=0;i<3;++i){setup::Control c;c.key=key++;c.kind=setup::Kind::picture;c.source={35+i*20,270,16,16};m.controls.push_back(c);}
    setup::Control heading;heading.key=key++;heading.kind=setup::Kind::label;
    heading.source={500,45,120,20};heading.text="Map name";m.controls.push_back(heading);
    setup::Control log;log.key=key++;log.kind=setup::Kind::label;
    log.source={40,350,380,120};log.text="Native status message";m.controls.push_back(log);
    return m;
}
static void setup_layouts() {
    auto model=setup_fixture();
    for(auto [w,h]:{std::pair{1920,1080},{1280,720},{2560,1080},{800,900}}) {
        for(int scroll:{0,3,99}) {
            model.controls[0].open=scroll!=0;
            const auto l=setup::arrange(w,h,model,scroll);
            std::cout << "setup " << w << ' ' << h << ' ' << scroll << ' ' << l.valid << ' '
                      << bits(l.scale) << ' ';
            rect(l.title);rect(l.players);rect(l.map);rect(l.preview);
            std::cout << l.controls.size() << ';';
            for(const auto& p:l.controls){std::cout << p.key << ',' << static_cast<int>(p.kind) << ',' << p.item << ':';rect(p.source);rect(p.target);}
            std::cout << '\n';
            int sx=-100,sy=-100;
            for(auto [x,y]:{std::pair{0,0},{w/2,h/2},{w-1,h-1}}) {
                const bool mapped=setup::map_point(l,x,y,sx,sy);
                std::cout << "point " << mapped << ' ' << sx << ' ' << sy << '\n';
            }
        }
    }
    model.controls[0].open=false;
    for (int canvas:{800,1200}) {
        const auto result=setup::arrange_native(1920,1080,canvas,canvas*3/4,model);
        std::cout << "native " << canvas << ' ' << result.widget_width << ' ' << result.geometry.valid << '\n';
    }
}
static void loading() {
    LoadingModel model;
    model.title="Mission";model.players={{"One","Federation",0,true,0.5f},{"Two","Klingon",1,false,1.0f}};
    model.show_preview=true;
    for (auto mode:{LoadingMode::unknown,LoadingMode::instant_action,LoadingMode::campaign,LoadingMode::multiplayer}) {
        model.kind=mode;
        for(auto [w,h]:{std::pair{800,600},{1280,800},{1920,1080},{3840,2160}}) {
            const auto l=loading_layout(w,h,model);
            std::cout << "loading " << static_cast<int>(mode) << ' ' << w << ' ' << h << ' ' << l.valid << ' '
                      << bits(l.scale) << ' ';
            for(const auto& r:{l.kicker,l.title,l.briefing,l.objectives,l.hint,l.preview,l.players,l.status,l.percentage})rect(r);
            std::cout << '\n';
        }
    }
    model.kind=LoadingMode::instant_action;
    model.players.assign(8,Player{"Long name","Species 8472",1,false,0.5f});
    model.protected_regions={{457,76,520,724},{40,770,1200,30},{188,104,190,180}};
    const auto l=loading_layout(1280,800,model);
    std::cout << "crowded " << l.valid << ' ' << bits(l.scale) << ' ';
    for(const auto& r:{l.kicker,l.title,l.briefing,l.preview,l.players,l.status,l.percentage})rect(r);
    std::cout << '\n';
}
static void content() {
    LoadingModel model;model.mode="Native";
    for(const std::string& source:{std::string{""},std::string{"Title\nFirst line\n second line\n\n* Goal\nHint: text\nmore"},
        std::string("Cit")+char(0xe9)+"\n*"+char(0xe9)}) {
        const bool good=parse_loading_objectives(source,model);
        std::cout << "parse " << good << ' ';text(model.title);text(model.mode);text(model.hint);
        std::cout << model.briefing.size() << ';';for(const auto& part:model.briefing)text(part);
        std::cout << model.objectives.size() << ';';for(const auto& part:model.objectives)text(part);
        std::cout << '\n';
    }
    for(const std::string path:{"Map.bzn","bzn/Map Name.BZN","../Map.bzn","Map.txt",".bzn","bad*name.bzn"}) {
        std::cout << "map ";text(path);text(sanitize_loading_map_name(path));std::cout << '\n';
    }
}
int main() {menu();setup_layouts();loading();content();}
