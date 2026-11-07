#pragma once
#include "presentation.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
namespace a2fo::refit_menu::menu_scene {
// CPU-only, called outside the short native-window copy lock. No extra GPU
// frame or input event is needed to fade the completed UI into its live scene.
inline std::shared_ptr<const Artwork> transition_image(std::shared_ptr<const Artwork> scene,
        const std::shared_ptr<const Artwork>& overlay,std::uint64_t elapsed) {
    constexpr std::uint64_t duration=180;
    const auto valid=[](const auto& image){return image&&image->width>0&&image->height>0&&
        std::uint64_t(image->width)*image->height==image->pixels.size();};
    if(!valid(overlay)||!valid(scene)||elapsed>=duration)return scene;
    if(!elapsed)return overlay;
    const unsigned mix=static_cast<unsigned>(elapsed*255/duration);
    auto output=std::make_shared<Artwork>();output->width=scene->width;output->height=scene->height;
    output->pixels.resize(scene->pixels.size());
    std::vector<int> columns(static_cast<std::size_t>(scene->width));
    for(int x=0;x<scene->width;++x)columns[x]=static_cast<int>(std::int64_t(x)*overlay->width/scene->width);
    for(int y=0;y<scene->height;++y){
        const auto row=static_cast<std::size_t>(std::int64_t(y)*overlay->height/scene->height)*overlay->width;
        for(int x=0;x<scene->width;++x){
            const auto i=static_cast<std::size_t>(y)*scene->width+x;
            const auto a=overlay->pixels[row+columns[x]],b=scene->pixels[i];
            const auto channel=[&](unsigned shift){return ((((a>>shift)&255u)*(255-mix)+
                ((b>>shift)&255u)*mix+127)/255)<<shift;};
            output->pixels[i]=channel(0)|channel(8)|channel(16);
        }
    }
    return output;
}
}
