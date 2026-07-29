#include "odf_paths.hpp"

#include <iostream>
#include <map>
#include <string>

namespace {

bool expect_alias(const std::string& request,
                  const std::map<std::string, std::string>& aliases,
                  const std::string& expected) {
    std::string actual;
    if (!a2fo::find_recursive_odf_alias(request, aliases, actual) ||
        actual != expected) {
        std::cerr << "expected alias " << request << " -> " << expected
                  << ", got " << actual << '\n';
        return false;
    }
    return true;
}

bool expect_native(const std::string& request,
                   const std::map<std::string, std::string>& aliases) {
    std::string actual = "unchanged";
    if (a2fo::find_recursive_odf_alias(request, aliases, actual) ||
        !actual.empty()) {
        std::cerr << "expected native lookup for " << request << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const std::map<std::string, std::string> aliases = {
        {"example.odf", "odf\\custom\\deep\\Example.odf"},
    };

    if (!expect_alias("odf\\ships\\example.odf", aliases,
                      "odf\\custom\\deep\\Example.odf") ||
        !expect_alias("ODF/Stations/EXAMPLE.ODF", aliases,
                      "odf\\custom\\deep\\Example.odf") ||
        !expect_alias("odf\\example.odf", aliases,
                      "odf\\custom\\deep\\Example.odf") ||
        !expect_alias("EXAMPLE.ODF", aliases,
                      "odf\\custom\\deep\\Example.odf") ||
        !expect_native("odf\\custom\\deep\\example.odf", aliases) ||
        !expect_native("textures\\example.odf", aliases) ||
        !expect_native("odf\\ships\\example.sod", aliases) ||
        !expect_native("odf\\ships\\missing.odf", aliases)) {
        return 1;
    }
    return 0;
}
