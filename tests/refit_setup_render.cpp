#include "../modules/A2FORefitMenus/setup_presentation.hpp"
#include "../modules/A2FORefitMenus/setup_skin.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef _WIN32
#error Win32 offscreen fixture
#endif
using namespace a2fo::refit_menu;
using namespace a2fo::refit_menu::setup;
static void check(bool value) { if(!value)throw std::runtime_error("GDI fixture failure"); }
static void require_pixel(const std::uint32_t* pixels,int width,int x,int y,
                          std::uint32_t expected,const std::string& name,int native_width) {
    const std::uint32_t actual=pixels[static_cast<std::size_t>(y)*width+x]&0x00ffffffu;
    if(actual!=expected) {
        throw std::runtime_error(name+" native "+std::to_string(native_width)+
            " pixel ("+std::to_string(x)+","+std::to_string(y)+") expected RGB "+
            std::to_string(expected)+" got "+std::to_string(actual));
    }
}
static const Placement& picture(const setup::Layout& layout,const Rect& source) {
    for(const auto& p:layout.controls)
        if(p.kind==Kind::picture&&p.source.x==source.x&&p.source.y==source.y&&
           p.source.width==source.width&&p.source.height==source.height)return p;
    throw std::runtime_error("missing native picture placement");
}
static void paint_region(Artwork& image,const Rect& logical,std::uint32_t rgb) {
    // 1200 is 3/2, so scale each edge before filling rather than truncating a factor.
    const int x0=logical.x*image.width/800,y0=logical.y*image.height/600;
    const int x1=(logical.x+logical.width)*image.width/800;
    const int y1=(logical.y+logical.height)*image.height/600;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)
        image.pixels[static_cast<std::size_t>(y)*image.width+x]=rgb;
}
static Artwork native_art(int width) {
    Artwork image{width,width*3/4,std::vector<std::uint32_t>(static_cast<std::size_t>(width)*(width*3/4),0x00101620u)};
    const std::uint32_t swatch[8]={0x00f12b35u,0x002ee450u,0x003b68f0u,0x00efbc29u,
                                   0x00d43de7u,0x002ad7d2u,0x00f18642u,0x0083ec5bu};
    const std::uint32_t palette[4]={0x00e92b71u,0x004de7b2u,0x00f2d84au,0x006b51ebu};
    for(int row=0;row<8;++row)paint_region(image,{30,25+row*28,16,16},swatch[row]);
    for(int chip=0;chip<4;++chip)paint_region(image,{30+chip*20,270,16,16},palette[chip]);
    // Unequal quadrants make both source axes and top-down orientation observable.
    paint_region(image,{627,129,71,83},0x00e64731u);
    paint_region(image,{698,129,84,83},0x003bdb74u);
    paint_region(image,{627,212,71,72},0x004866e9u);
    paint_region(image,{698,212,84,72},0x00ecd245u);
    return image;
}
static void check_native_pictures(const setup::Layout& layout,const std::uint32_t* pixels,
                                  int width,int native_width) {
    const std::uint32_t swatch[8]={0x00f12b35u,0x002ee450u,0x003b68f0u,0x00efbc29u,
                                   0x00d43de7u,0x002ad7d2u,0x00f18642u,0x0083ec5bu};
    const std::uint32_t palette[4]={0x00e92b71u,0x004de7b2u,0x00f2d84au,0x006b51ebu};
    for(int row=0;row<8;++row) {
        const Rect& dst=picture(layout,{30,25+row*28,16,16}).target;
        require_pixel(pixels,width,dst.x+dst.width/2,dst.y+dst.height/2,
                      swatch[row],"player swatch row "+std::to_string(row),native_width);
    }
    for(int chip=0;chip<4;++chip) {
        const Rect& dst=picture(layout,{30+chip*20,270,16,16}).target;
        require_pixel(pixels,width,dst.x+dst.width/2,dst.y+dst.height/2,
                      palette[chip],"palette chip "+std::to_string(chip),native_width);
    }
    const Rect& dst=picture(layout,{627,129,155,155}).target;
    const std::uint32_t map[4]={0x00e64731u,0x003bdb74u,0x004866e9u,0x00ecd245u};
    for(int quadrant=0;quadrant<4;++quadrant)
        require_pixel(pixels,width,dst.x+(quadrant%2?3:1)*dst.width/4,
                      dst.y+(quadrant/2?3:1)*dst.height/4,map[quadrant],
                      "minimap quadrant "+std::to_string(quadrant),native_width);
}
static Model fixture() {
    Model m;std::uintptr_t key=1;
    for(int row=0;row<8;++row)for(int col=0;col<3;++col) {
        Control c;c.key=key++;c.kind=Kind::combo;c.source={60+col*130,25+row*28,112,22};
        c.text=col==0?"Federation":col==1?"Team 1":"Normal";
        c.pull=c.source;c.item_height=22;c.items={"Federation","Klingon","Romulan"};
        if(row==7&&col==0)c.open=true;
        if(row==5&&col==2)c.enabled=false;
        m.controls.push_back(c);
    }
    for(int row=0;row<8;++row){Control c;c.key=key++;c.kind=Kind::picture;c.source={30,25+row*28,16,16};m.controls.push_back(c);}
    for(int i=0;i<2;++i){Control c;c.key=key++;c.kind=Kind::button;c.source={400+i*150,580,120,25};c.text=i?"Launch Game":"Back";m.controls.push_back(c);}
    for(int i=0;i<6;++i){Control c;c.key=key++;c.kind=Kind::button;c.source={400+(i/3)*125,320+(i%3)*45,110,25};c.text=i%2?"On":"Off";m.controls.push_back(c);}
    for(int i=0;i<4;++i){Control c;c.key=key++;c.kind=Kind::picture;c.source={30+i*20,270,16,16};m.controls.push_back(c);}
    Control map;map.key=key++;map.kind=Kind::label;map.source={500,60,150,20};map.text="Nebula Crossroads";m.controls.push_back(map);
    Control log;log.key=key++;log.kind=Kind::label;log.source={40,350,380,140};
    log.text="A map has been selected.\nPlayer configuration is ready.";m.controls.push_back(log);
    return m;
}
static void save(const char* path,int w,int h,const std::uint32_t* pixels) {
    BITMAPFILEHEADER fh{};BITMAPINFOHEADER ih{};
    fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+w*h*4;
    ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=-h;ih.biPlanes=1;ih.biBitCount=32;ih.biSizeImage=w*h*4;
    std::ofstream f(path,std::ios::binary);check(bool(f));
    f.write(reinterpret_cast<char*>(&fh),sizeof(fh));f.write(reinterpret_cast<char*>(&ih),sizeof(ih));
    f.write(reinterpret_cast<const char*>(pixels),w*h*4);check(bool(f));
}
static void check_transparency(){
    check(setup_skin::available()); // Fail rather than silently test the GDI fallback.
    HDC dc=CreateCompatibleDC(nullptr);check(dc!=nullptr);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=400;info.bmiHeader.biHeight=-120;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bytes{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bytes,nullptr,0);check(bitmap&&bytes);
    auto old=SelectObject(dc,bitmap);check(old&&old!=HGDI_ERROR);
    auto* pixels=static_cast<std::uint32_t*>(bytes);
    const unsigned rgb[8]={0x101820,0x27343f,0x263744,0x365064,0x7a5630,0x202933,0x101820,0x365064};
    const unsigned alpha[8]={180,210,220,220,220,180,255,255};
    for(const auto size:{std::pair{96,48},std::pair{321,83}})
        for(const unsigned background:{0x112233u,0xc09050u})for(unsigned part=0;part<8;++part){
            GdiFlush();std::fill_n(pixels,400*120,background);
            const Rect rectangle{4,4,size.first,size.second};
            check(setup_skin::paint(dc,rectangle,static_cast<setup_skin::Part>(part),8));
            check(GdiFlush()!=0);
            require_pixel(pixels,400,4,4,background,"transparent PNG corner",size.first);
            const auto center=pixels[static_cast<std::size_t>(4+size.second/2)*400+4+size.first/2];
            for(unsigned shift:{0u,8u,16u}){
                const int expected=static_cast<int>((((rgb[part]>>shift)&255)*alpha[part]+127)/255+
                    (((background>>shift)&255)*(255-alpha[part])+127)/255);
                const int actual=static_cast<int>((center>>shift)&255);
                if(std::abs(expected-actual)>2)throw std::runtime_error("PNG component alpha/channel mismatch");
            }
        }
    check(!setup_skin::paint(dc,{0,0,40,30},setup_skin::Part::count,8));
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    std::cout<<"refit transparent PNG components OK (all states, two backgrounds, repeated cached sizes)\n";
}
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"usage: refit_setup_render.exe large.bmp small.bmp\n";return 2;}
    try {
        check_transparency();
        Model m=fixture();
        for(int native_width:{800,1200})for(int n=0;n<2;++n){int w=n?1280:1920,h=n?720:1080;
            HDC dc=CreateCompatibleDC(nullptr);check(dc!=nullptr);
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;
            info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            void* bytes=nullptr;HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bytes,nullptr,0);check(bmp&&bytes);
            HGDIOBJ old=SelectObject(dc,bmp);check(old&&old!=HGDI_ERROR);
            Snapshot seed=default_snapshot();
            check(a2fo::refit_menu::paint(dc,w,h,seed,-1,-1)); // Registers embedded private fonts.
            Artwork scene{w,h,std::vector<std::uint32_t>(static_cast<std::size_t>(w)*h,0x00163350u)};
            Artwork native=native_art(native_width);
            Model raw=m;
            const auto edge=[native_width](int v){return static_cast<int>(std::lround(v*native_width/800.0));};
            const auto rectangle=[&](Rect r){
                const int right=edge(r.x+r.width),bottom=edge(r.y+r.height);
                r.x=edge(r.x);r.y=edge(r.y);r.width=right-r.x;r.height=bottom-r.y;return r;
            };
            for(auto& control:raw.controls){
                control.source=rectangle(control.source);control.pull=rectangle(control.pull);
                control.item_height=edge(control.item_height);
            }
            const auto normalized=arrange_native(w,h,native_width,native_width*3/4,raw);
            const auto& l=normalized.geometry;
            check(l.valid&&normalized.widget_width==native_width);
            check(setup::paint(dc,w,h,normalized.model,l,&scene,&native,l.players.x+20,l.players.y+100));
            check(GdiFlush()!=0);
            const auto* reference_pixels=static_cast<const std::uint32_t*>(bytes);
            std::vector<std::uint32_t> reference(reference_pixels,reference_pixels+static_cast<std::size_t>(w)*h);
            {
                setup_skin::DirectTargetScope direct{dc,w,h,bytes};
                check(setup::paint(dc,w,h,normalized.model,l,&scene,&native,l.players.x+20,l.players.y+100));
                check(GdiFlush()!=0);
            }
            check(std::equal(reference.begin(),reference.end(),static_cast<const std::uint32_t*>(bytes)));
            check_native_pictures(l,static_cast<std::uint32_t*>(bytes),w,native_width);
            save(n?argv[2]:argv[1],w,h,static_cast<std::uint32_t*>(bytes));
            SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
        }
        std::cout<<"refit setup render OK\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
