#include "../modules/A2FORefitMenus/loading_theme.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace a2fo::refit_menu;
namespace fs = std::filesystem;

static void write(const fs::path& root, const std::string& value) {
    const auto folder = root / "RefitScene" / "Loading";
    fs::create_directories(folder);
    std::ofstream output(folder / "themes.ini");
    output << value;
    assert(bool(output));
}

int main() {
    const fs::path temp = fs::temp_directory_path() / "refit_loading_theme_test";
    fs::remove_all(temp);
    const auto low = temp / "low", high = temp / "high";
    std::ifstream source("assets/refit-menus/loading/themes.ini");
    assert(bool(source));
    std::ostringstream stock;
    stock << source.rdbuf();
    write(low, stock.str());
    const auto roots = std::vector<std::string>{low.string(), high.string()};
    const auto card = loading_theme(roots, "CARD");
    const auto organic = loading_theme(roots, "Species 8472");
    assert(card.name == "Cardassian" && card.form == LoadingForm::arch);
    assert(card.accent == 0xffe8a438u && card.corners[0] == -18.0f);
    assert(organic.name == "Species 8472" && organic.form == LoadingForm::organic);
    assert(organic.accent == 0xffc4a2ffu && organic.corners[0] == 26.0f);
    const auto dominion=loading_theme(roots,"dom");
    assert(dominion.name=="Dominion"&&dominion.accent==0xffcc99ffu&&dominion.form==LoadingForm::blade);
    assert(loading_theme(roots, "Unlisted Empire").form == LoadingForm::shell);
    const auto klingon=loading_theme(roots,"klingonempire");
    assert(klingon.name=="Klingon"&&klingon.form==LoadingForm::blade&&klingon.accent==0xffd8261cu);

    write(high, "[Theme.cardassian]\naccent=ff112233\n");
    assert(loading_theme(roots, "cardassian").accent == 0xff112233u);
    write(high, "[Theme.cardassian]\naccent=invalid!\n");
    assert(loading_theme(roots, "cardassian").form == LoadingForm::shell);

    std::string custom = stock.str().substr(stock.str().find("[Theme.cardassian]"));
    custom = custom.substr(0, custom.find("[Theme.borg]"));
    custom.replace(0, std::string("[Theme.cardassian]").size(), "[Theme.unknown_empire]");
    write(high, custom);
    const auto added = loading_theme(roots, "unknown_empire");
    assert(added.race == "unknown_empire" && added.form == LoadingForm::arch);
    assert(added.accent == 0xffe8a438u);
    fs::remove_all(temp);
    std::cout << "refit loading theme registry OK\n";
}
