#include "edit_menu_odf.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "edit_menu_test: " << message << '\n';
    std::exit(1);
}

}  // namespace

int main() {
    using a2fo::edit_menu::MenuNode;

    require(a2fo::edit_menu::normalize_odf_name("ODF/Other/E_Fed") ==
                "e_fed.odf",
            "ODF names should normalize to lowercase basenames");

    const std::string submenu_odf = R"ODF(
        // A quoted comment marker is data: "//"
        menuTitle = "Federation // Ships"
        buildItem1 = "EF_Combat.ODF"
        buildItem12 = "odf/other/EF_Support"
        item1 = "ignored_for_submenu_detection"
        forceToNeutral = true
    )ODF";
    MenuNode submenu{};
    std::string error;
    require(a2fo::edit_menu::parse_menu_node(
                submenu_odf, "EF_SHIPS.odf", &submenu, &error),
            "submenu ODF should parse");
    require(error.empty(), "successful parse should not report an error");
    require(submenu.source_name == "ef_ships.odf",
            "source name should normalize");
    require(submenu.title == "Federation // Ships",
            "quoted comment markers should survive");
    require(submenu.build_items[0] == "ef_combat.odf",
            "buildItem1 should parse");
    require(submenu.build_items[11] == "ef_support.odf",
            "buildItem12 should parse");
    require(submenu.is_submenu(),
            "any buildItem should classify a node as a submenu");
    require(submenu.force_to_neutral,
            "textual forceToNeutral should parse");

    const std::string leaf_odf = R"ODF(
        /* buildItem1 = "commented_out.odf" */
        menuTitle = "Combat Ships"
        item1 = "fbattle"
        item2 = "fcruise1" // trailing comment
        item12 = fscout;
        forceToNeutral = 0
    )ODF";
    MenuNode leaf{};
    require(a2fo::edit_menu::parse_menu_node(
                leaf_odf, "ef_combat", &leaf, &error),
            "leaf ODF should parse");
    require(!leaf.is_submenu(),
            "item-only nodes should remain placement lists");
    require(leaf.items[0] == "fbattle" && leaf.items[1] == "fcruise1" &&
                leaf.items[11] == "fscout",
            "quoted and bare item values should parse");
    require(!leaf.force_to_neutral,
            "numeric forceToNeutral should parse");

    std::cout << "edit menu parser tests passed\n";
    return 0;
}
