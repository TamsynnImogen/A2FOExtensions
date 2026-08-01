#include "extension_roots.hpp"

#include <string>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string command = "ArmadaL.exe";
    if (argc >= 3 && argv[2][0] != '\0') {
        command += " /mod \"" + std::string(argv[2]) + "\"";
    }
    const a2fo::ExtensionRootDiscovery result =
        a2fo::discover_extension_roots(argv[1], command);
    if (result.roots.empty() || result.roots.front() != argv[1]) return 2;
    if (argc >= 3 && argv[2][0] != '\0') {
        if (result.active_mod != argv[2]) return 3;
    }
    if (argc >= 4 && argv[3][0] != '\0') {
        bool parent_found = false;
        for (const std::string& root : result.roots) {
            if (root.find(argv[3]) != std::string::npos) parent_found = true;
        }
        if (!parent_found) return 4;
    }
}
