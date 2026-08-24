#include "../modules/A2FOHybridBuild/build_submenu_config.hpp"

#include <cstdio>
#include <string>

namespace {

bool require(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "build submenu config test failed: %s\n", message);
    return false;
}

}  // namespace

int main() {
    const std::string source = R"ODF(
        buildItem0 = "fscout"
        buildItem1 = "fexcelsiormenu"
        buildItem1Refit0 = "FExcelsior.odf"
        buildItem1Refit1 = "odf/ships/FExcelsiorII"
        buildItem4Refit2 = funarmed
        // buildItem2Refit0 = "commented"
        /* buildItem3Refit0 = "also_commented" */
    )ODF";

    a2fo::build_submenu::Config config;
    std::string error;
    if (!require(a2fo::build_submenu::parse_config(
                     source, &config, &error),
                 "parser rejected valid commands") ||
        !require(!config.empty(), "config should contain submenus") ||
        !require(config.pages[0].empty(),
                 "ordinary buildItem must not become a submenu") ||
        !require(config.pages[1].children[0] == "fexcelsior",
                 "first variant should normalize") ||
        !require(config.pages[1].children[1] == "fexcelsiorii",
                 "path and extension should normalize") ||
        !require(config.pages[4].children[2] == "funarmed",
                 "sparse child index should parse") ||
        !require(config.pages[2].empty() && config.pages[3].empty(),
                 "comments must be ignored")) {
        return 1;
    }

    a2fo::build_submenu::Config invalid_only;
    if (!require(a2fo::build_submenu::parse_config(
                     "buildItem57Refit0 = x\n"
                     "buildItem1Refit57 = y\n",
                     &invalid_only),
                 "out-of-range commands should be harmless") ||
        !require(invalid_only.empty(),
                 "out-of-range commands must not be registered")) {
        return 2;
    }
    return 0;
}
