#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
namespace native11_fixture {
inline std::vector<std::uint8_t> model(const char* texture="",bool fullbright=false) {
    std::vector<std::uint8_t> bytes;
    auto half=[&](std::uint16_t v){bytes.push_back(v&255);bytes.push_back(v>>8);};
    auto full=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)bytes.push_back((v>>(i*8))&255);};
    auto real=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);full(bits);};
    auto text=[&](const char* value){half(static_cast<std::uint16_t>(std::strlen(value)));while(*value)bytes.push_back(*value++);};
    const char magic[]="Storm3D_SW";bytes.insert(bytes.end(),magic,magic+10);real(1.93f);
    half(1);text("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})real(v);
    bytes.push_back(2);bytes.push_back(fullbright?1:0);half(1);half(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})real(v);
    text("default");full(4);full(1);text(texture);full(0);text("");half(0);
    half(3);half(3);half(1);
    for(float v:{-.75f,-.75f,.2f,.75f,-.75f,.2f,0.f,.75f,.2f})real(v);
    for(unsigned i=0;i<6;++i)real(0);
    half(1);text("mat");for(unsigned i=0;i<3;++i){half(i);half(i);}
    bytes.push_back(0);half(0);half(0);half(0);return bytes;
}
}
