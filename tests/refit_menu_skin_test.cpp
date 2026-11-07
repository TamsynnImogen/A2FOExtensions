#include "../modules/A2FORefitMenus/skin.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

using a2fo::refit_menu::skin::Role;
using a2fo::refit_menu::skin::apply;
using a2fo::refit_menu::skin::classify;

int main() {
    for (const char* path : {
             "bitmaps\\main\\mainbkgr.png", "Backgrounds/OptionsBG.bmp",
             "bitmaps/Backgrounds/GraphicBG.png", "MissionSelect/MissionSelectBG.bmp",
             "bitmaps/multisetup/instactionbkg.png", "multisetup/MultiplaySetupBkg.png",
             "multisetup/advanced/ADVANCEbkg.png", "multisetup/mapselect/MapSelectBkg.png",
             "multiconnect/MultiBackground.png", "multiconnect/Internetcon.png",
             "AdmiralsLog/BACKGROUND.png", "single/singleplay.png"})
        assert(classify(path).role == Role::background);

    assert(classify("SharedButtons/Button1_hoover.bmp").hot);
    assert(classify("SharedButtons/Button1_down.png").pressed);
    assert(classify("SharedButtons/middleMediumBut_dis.png").disabled);
    assert(classify("SharedButtons/scrollTopH_small.png").hot);
    assert(classify("SharedButtons/sliders/HThumb.bmp").role == Role::widget);
    assert(classify("SharedButtons/ScrollBars/medscrolltableft_high.png").hot);
    assert(classify("SharedButtons/scrollbars/medscrolbottH.png").hot);
    assert(classify("MissionSelect/MissionFieldDisabled_small.bmp").disabled);
    assert(classify("MissionSelect/MissionFieldH.bmp").hot);
    assert(classify("multisetup/buttons/opponentbutts/oppobuttnorm_high.png").hot);
    assert(classify("multisetup/advanced/ADVtextbutthilitR.png").hot);
    assert(classify("multisetup/advanced/ADVtextbuttnormR_high.png").hot);
    assert(classify("multisetup/advanced/ADVbuttcanhilit.png").hot);
    assert(classify("multisetup/advanced/ADVscrollslidhilit.png").hot);
    assert(classify("multisetup/mapselect/Lmapselectbuttsnorm_high.png").hot);
    assert(classify("multisetup/buttons/opttabbotthilit.png").hot);
    assert(classify("multiconnect/launchhilitSMALL.png").hot);
    assert(classify("multiconnect/CHEKON.bmp").role == Role::widget);

    for (const char* path : {
             "/bitmaps/main/mainbkgr.png", "C:\\bitmaps\\main\\mainbkgr.png",
             "bitmaps/../main/mainbkgr.png", "bitmaps//main/mainbkgr.png",
             "bitmaps/main/mainbkgr.dds", "textures/main/mainbkgr.png",
             "Backgrounds/OptionsBG_small.png",
             "AdmiralsLog/fed_galaxyr4.bmp", "single/BORGGRAY.png",
             "multiconnect/techtree.png", "multisetup/unknownbkg.png",
             "map/MapSelectBkg.png", "MissionSelect/missionpreview.png"})
        assert(classify(path).role == Role::none);

    std::array<std::uint32_t, 8> pixels = {
        0xff336699u, 0x00aabbccu, 0xffff00ffu, 0x80808080u,
        0xffe8e8e8u, 0xff101010u, 0xff336699u, 0xff336699u};
    const auto original = pixels;
    assert(!apply(pixels.data(), 4, 3, 2, classify("unknown.png")));
    assert(pixels == original);
    assert(!apply(pixels.data(), 2, 3, 2, classify("main/mainbkgr.png")));
    assert(!apply(pixels.data(), 4, -1, 2, classify("main/mainbkgr.png")));
    assert(!apply(pixels.data(), 8193, 8193, 1, classify("main/mainbkgr.png")));
    assert(!apply(pixels.data(), 4, 1, 8193, classify("main/mainbkgr.png")));
    assert(!apply(pixels.data(), 8192, 8192, 8192, classify("main/mainbkgr.png")));
    assert(!apply(pixels.data(), std::numeric_limits<std::size_t>::max() /
                  sizeof(std::uint32_t), 1, 2, classify("main/mainbkgr.png")));
    assert(!apply(pixels.data(), std::numeric_limits<std::size_t>::max(), 3, 3,
                  classify("main/mainbkgr.png")));
    assert(!apply(nullptr, 4, 3, 2, classify("main/mainbkgr.png")));
    assert(pixels == original);
    assert(apply(pixels.data(), 4, 3, 2, classify("main/mainbkgr.png")));
    assert(pixels[0] != original[0]);
    assert(pixels[1] == original[1]);
    assert(pixels[2] == original[2]);
    assert((pixels[3] & 0xff000000u) == 0x80000000u);
    assert(pixels[7] == original[7]); // stride padding is untouched

    std::array<std::uint32_t, 3> normal = {0xff306080u, 0xffeeeeeeu, 0xffff00ffu};
    auto hot = normal, pressed = normal, disabled = normal;
    assert(apply(normal.data(), 3, 3, 1, classify("SharedButtons/Button1.png")));
    assert(apply(hot.data(), 3, 3, 1, classify("SharedButtons/Button1_hoover.png")));
    assert(apply(pressed.data(), 3, 3, 1, classify("SharedButtons/Button1_down.png")));
    assert(apply(disabled.data(), 3, 3, 1, classify("SharedButtons/Button1_disabled.png")));
    assert(normal[0] != hot[0] && normal[0] != pressed[0] && normal[0] != disabled[0]);
    assert(hot[0] != pressed[0]);
    assert(normal[1] > 0xffb0b0b0u); // light bitmap text stays light
    assert(normal[2] == 0xffff00ffu);
}
