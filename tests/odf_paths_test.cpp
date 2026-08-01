#include "odf_paths.hpp"

#include <cassert>
#include <map>
#include <string>

int main() {
    using a2fo::recursive_odf_basename_key;
    assert(recursive_odf_basename_key("Example.ODF") == "example.odf");
    assert(recursive_odf_basename_key("odf/custom/Ship.odf") == "ship.odf");
    assert(recursive_odf_basename_key("textures/Ship.odf").empty());
    assert(recursive_odf_basename_key("ship.sod").empty());

    const std::map<std::string, std::string> aliases{
        {"ship.odf", "odf\\custom\\ship.odf"}};
    std::string selected;
    assert(a2fo::find_recursive_odf_alias("ship.odf", aliases, selected));
    assert(selected == "odf\\custom\\ship.odf");
    assert(!a2fo::find_recursive_odf_alias(
        "odf/custom/ship.odf", aliases, selected));
}
