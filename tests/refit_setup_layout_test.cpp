#include "../modules/A2FORefitMenus/setup_presentation.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>

using namespace a2fo::refit_menu;
using namespace a2fo::refit_menu::setup;

static Model fixture() {
    Model m;
    std::uintptr_t key=1;
    for(int row=0;row<8;++row)for(int col=0;col<3;++col) {
        Control c;
        c.key=key++;c.kind=Kind::combo;c.source={50+col*125,20+row*28,100,22};
        c.text="Selected";c.items={"One","Two","Three","Four","Five","Six","Seven","Eight","Nine","Ten"};
        c.pull={c.source.x,c.source.y,100,22};c.item_height=22;
        m.controls.push_back(c);
    }
    for(int i=0;i<8;++i) {
        Control c;c.key=key++;c.kind=Kind::picture;c.source={30,20+i*28,16,16};
        m.controls.push_back(c);
    }
    for(int i=0;i<2;++i) {
        Control c;c.key=key++;c.kind=Kind::button;c.source={400+i*140,580,120,28};
        c.text=i?"Launch":"Back";m.controls.push_back(c);
    }
    for(int i=0;i<3;++i) {
        Control c;c.key=key++;c.kind=Kind::picture;c.source={35+i*20,270,16,16};
        m.controls.push_back(c);
    }
    Control heading;heading.key=key++;heading.kind=Kind::label;
    heading.source={500,45,120,20};heading.text="Map name";m.controls.push_back(heading);
    Control log;log.key=key++;log.kind=Kind::label;
    log.source={40,350,380,120};log.text="Native status message";m.controls.push_back(log);
    return m;
}
static const Placement* find(const setup::Layout& l,std::uintptr_t key,int item=-1) {
    for(const auto& p:l.controls)if(p.key==key&&p.item==item)return &p;
    return nullptr;
}
static Artwork native_pattern(int width,int height) {
    Artwork image;
    image.width=width;image.height=height;
    image.pixels.resize(static_cast<std::size_t>(width)*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        image.pixels[static_cast<std::size_t>(y)*width+x]=
            (static_cast<std::uint32_t>(x)<<12)|static_cast<std::uint32_t>(y);
    return image;
}
static void check_preview_snapshot() {
    setup::Layout layout;
    layout.valid=true;
    layout.controls.push_back({1,{11,12,16,16},{0,0,16,16},Kind::picture,-2});
    layout.controls.push_back({2,{627,129,155,155},{0,0,155,155},Kind::picture,-1});
    for(const int native_width:{800,1200}) {
        const int native_height=native_width*3/4;
        const Artwork native=native_pattern(native_width,native_height);
        const Artwork result=loading_preview_snapshot(native,layout);
        const int x=627*native_width/800,y=129*native_height/600;
        const int width=155*native_width/800,height=155*native_height/600;
        assert(result.width==width&&result.height==height);
        assert(result.pixels.front()==native.pixels[static_cast<std::size_t>(y)*native_width+x]);
        assert(result.pixels[width-1]==native.pixels[static_cast<std::size_t>(y)*native_width+x+width-1]);
        assert(result.pixels.back()==native.pixels[static_cast<std::size_t>(y+height-1)*native_width+x+width-1]);
        assert(result.pixels.front()!=native.pixels[static_cast<std::size_t>(12*native_height/600)*native_width+11*native_width/800]);
    }
    const Artwork native=native_pattern(800,600);
    const auto arranged=arrange(1920,1080,fixture());
    assert(arranged.valid);
    const Artwork arranged_preview=loading_preview_snapshot(native,arranged);
    assert(arranged_preview.width==155&&arranged_preview.height==155);
    assert(arranged_preview.pixels.front()==native.pixels[static_cast<std::size_t>(129)*800+627]);
    layout.controls.pop_back(); // Item -2 alone is not a preview.
    assert(loading_preview_snapshot(native,layout).pixels.empty());
    layout.controls.push_back({2,{627,129,155,155},{0,0,155,155},Kind::picture,-1});
    layout.valid=false;
    assert(loading_preview_snapshot(native,layout).pixels.empty());
    layout.valid=true;
    layout.controls.back().source={790,129,155,155};
    assert(loading_preview_snapshot(native,layout).pixels.empty());
    layout.controls.back().source={627,129,155,155};
    Artwork incomplete=native;incomplete.pixels.pop_back();
    assert(loading_preview_snapshot(incomplete,layout).pixels.empty());
}
int main() {
    check_preview_snapshot();
    {
        auto model=fixture();auto geometry=arrange(1920,1080,model);
        auto launch=std::find_if(model.controls.begin(),model.controls.end(),[](const auto& c){return c.text=="Launch";});
        const auto* button=find(geometry,launch->key);assert(button);
        const int x=button->target.x+button->target.width/2,y=button->target.y+button->target.height/2;
        launch->text="Localized launch";
        assert(launch_at(model,geometry,x,y)==launch->key&&launch_at(model,geometry,0,0)==0);
        launch->enabled=false;assert(!launch_at(model,geometry,x,y));launch->enabled=true;
        model.controls[0].open=true;assert(!launch_at(model,geometry,x,y));model.controls[0].open=false;
        auto extra=*launch;extra.key=9999;model.controls.push_back(extra);
        assert(!launch_at(model,geometry,x,y));
        LaunchPress press;press.key=123;press.cancel();assert(!press.release(123));
        press.key=123;assert(!press.release(456)&&!press.release(123));
        press.key=123;assert(press.release(123)&&!press.release(123));
    }
    Model m=fixture();
    for(auto size: {std::pair{1920,1080},std::pair{1280,720},std::pair{2560,1080},std::pair{800,900}}) {
        setup::Layout l=arrange(size.first,size.second,m);
        assert(l.valid&&l.scale>0);
        assert(l.players.x<l.map.x&&l.players.y==l.map.y);
        if(size.first==2560)assert(l.title.x>200);
        assert(l.map.y+l.map.height<=size.second);
        assert(l.controls.size()==40); // Includes the native preview crop.
        assert(l.players.y<l.map.y+l.map.height);
        assert(l.players.height>0 && l.players.y+l.players.height<l.map.y+l.map.height);
        assert(l.preview.width>=l.map.width/3);
        assert(l.controls.front().key==UINTPTR_MAX);
        assert(l.controls.front().source.x==627);
        assert(find(l,35)&&find(l,36)&&find(l,37));
        assert(find(l,35)->target.y>find(l,22)->target.y);
        assert(find(l,33)->target.y>=l.map.y+l.map.height);
        assert(find(l,33)->target.x<l.map.x);
        assert(find(l,34)->target.x>=l.map.x);
        assert(find(l,1)->target.x>l.players.x+60*l.scale);
        for(const auto& p:l.controls) {
            assert(p.target.x>=0&&p.target.y>=0&&p.target.x+p.target.width<=size.first);
            assert(p.target.y+p.target.height<=size.second);
        }
        int x=-1,y=-1;
        auto nav=find(l,33);assert(nav);
        assert(map_point(l,nav->target.x+nav->target.width/2,nav->target.y+nav->target.height/2,x,y));
        assert(x>=nav->source.x&&x<nav->source.x+nav->source.width);
        assert(y>=nav->source.y&&y<nav->source.y+nav->source.height);
        assert(!map_point(l,0,0,x,y));
        auto label=find(l,38);assert(label);
        assert(!map_point(l,label->target.x+3,label->target.y+3,x,y));
        auto preview=find(l,UINTPTR_MAX);assert(preview);
        assert(map_point(l,preview->target.x+preview->target.width/2,
                         preview->target.y+preview->target.height/2,x,y));
        assert(x>=627&&x<782&&y>=129&&y<284);
    }
    auto stock_on_fo=arrange_native(1920,1080,1200,900,m);
    assert(stock_on_fo.geometry.valid&&stock_on_fo.widget_width==800);
    Model fo=m;
    for(auto& c:fo.controls){
        const auto scale=[](int n){return n*3/2;};
        c.source={scale(c.source.x),scale(c.source.y),scale(c.source.width),scale(c.source.height)};
        c.pull={scale(c.pull.x),scale(c.pull.y),scale(c.pull.width),scale(c.pull.height)};
        c.item_height=scale(c.item_height);
    }
    auto fo_layout=arrange_native(1920,1080,1200,900,fo);
    assert(fo_layout.geometry.valid&&fo_layout.widget_width==1200);
    Model with_message=m;
    Control message;message.key=102;message.kind=Kind::label;message.message=true;
    message.source={682,341,100,140};message.text="Read-only native message";
    with_message.controls.push_back(message);
    const auto check_messages=[](const setup::Layout& layout) {
        assert(layout.valid);
        const auto* left=find(layout,39),*right=find(layout,102);
        assert(left&&right);
        for(const auto* log:{left,right}) {
            assert(log->target.x==layout.players.x);
            assert(log->target.y>=layout.players.y+layout.players.height);
            assert(log->target.x+log->target.width<=layout.map.x);
            assert(log->target.y+log->target.height<=layout.map.y+layout.map.height);
            int sx=-1,sy=-1;
            assert(!map_point(layout,log->target.x+log->target.width/2,
                              log->target.y+log->target.height/2,sx,sy));
        }
        assert(left->target.y+left->target.height<=right->target.y ||
               right->target.y+right->target.height<=left->target.y);
        assert(right->target.y>layout.players.y+layout.players.height);
    };
    check_messages(arrange(1920,1080,with_message));
    auto stock_message=arrange_native(1920,1080,1200,900,with_message);
    assert(stock_message.geometry.valid&&stock_message.widget_width==800);
    check_messages(stock_message.geometry);
    Model fo_with_message=fo;
    message.source={1023,511,150,210};fo_with_message.controls.push_back(message);
    auto fo_message=arrange_native(1920,1080,1200,900,fo_with_message);
    assert(fo_message.geometry.valid&&fo_message.widget_width==1200);
    assert(fo_message.model.controls.back().source.x==682);
    check_messages(fo_message.geometry);
    assert(!arrange_native(1920,1080,1000,750,m).geometry.valid);
    assert(!arrange_native(1920,1080,800,600,fo).geometry.valid);
    Model no_chips=m;no_chips.controls.erase(no_chips.controls.begin()+24,no_chips.controls.begin()+32);
    setup::Layout synthetic=arrange(1920,1080,no_chips);
    assert(synthetic.valid);
    int generated=0;for(const auto& p:synthetic.controls)if(p.item==-2)++generated;
    assert(generated==8);
    for(const auto& p:synthetic.controls)if(p.item==-2){
        int x{},y{};assert(map_point(synthetic,p.target.x+2,p.target.y+2,x,y));
        assert(x>=30&&x<46);
    }
    Model with_heading=m;Control player_heading;player_heading.key=101;player_heading.kind=Kind::label;
    player_heading.source={41,55,120,20};player_heading.text="Player Name";with_heading.controls.push_back(player_heading);
    auto headed=arrange(1920,1080,with_heading);assert(headed.valid);
    assert(find(headed,101)->target.y<find(headed,1)->target.y);
    Control native_title;native_title.key=100;native_title.kind=Kind::label;
    native_title.source={5,5,250,20};native_title.text="Native Setup";
    m.controls.push_back(native_title);
    assert(find(arrange(1920,1080,m),100)->target.y<100);
    m.controls.pop_back();
    Model high_message=m;
    message.source={600,5,150,100};high_message.controls.push_back(message);
    auto high_layout=arrange(1920,1080,high_message);assert(high_layout.valid);
    assert(find(high_layout,102)->target.y>high_layout.players.y+high_layout.players.height);
    Model with_random=m;Control random;random.key=91;random.kind=Kind::button;
    random.source={100,280,120,28};random.text="Random";with_random.controls.push_back(random);
    auto palette_layout=arrange(1920,1080,with_random);
    assert(palette_layout.valid);
    assert(find(palette_layout,35)->target.x>=
           find(palette_layout,91)->target.x+find(palette_layout,91)->target.width);
    int palette_x{},palette_y{};
    const auto* chip=find(palette_layout,35);assert(chip);
    assert(map_point(palette_layout,chip->target.x+2,chip->target.y+2,palette_x,palette_y));
    assert(palette_x>=chip->source.x&&palette_x<chip->source.x+chip->source.width);
    Control setting;setting.key=92;setting.kind=Kind::combo;setting.source={500,320,100,20};
    setting.open=true;setting.pull={500,320,100,20};setting.item_height=20;setting.items={"A","B"};
    Model setting_model=m;setting_model.controls.push_back(setting);
    auto setting_layout=arrange(1920,1080,setting_model);assert(setting_layout.valid);
    assert(find(setting_layout,92,0)&&find(setting_layout,92,1));
    assert(!arrange(320,200,m).valid);
    Model missing=m;missing.controls.erase(missing.controls.begin());
    assert(!arrange(1920,1080,missing).valid);
    Model unknown=m;Control orphan;orphan.key=99;orphan.kind=Kind::button;orphan.source={100,400,40,20};unknown.controls.push_back(orphan);
    assert(!arrange(1920,1080,unknown).valid);
    m.controls[21].open=true;m.controls[21].selected=9;m.controls[21].enabled=false;
    setup::Layout popup=arrange(1280,720,m,100);assert(popup.valid);
    assert(find(popup,22));
    assert(find(popup,22,2)&&find(popup,22,9)&&!find(popup,22,1));
    auto row=find(popup,22,9);int sx=-1,sy=-1;
    assert(map_point(popup,row->target.x+row->target.width/2,row->target.y+row->target.height/2,sx,sy));
    assert(sx>=row->source.x&&sx<row->source.x+row->source.width);
    assert(sy>=row->source.y&&sy<row->source.y+row->source.height);
    assert(row->source.y==m.controls[21].pull.y+10*m.controls[21].item_height);
    std::cout<<"refit setup layout OK\n";
}
