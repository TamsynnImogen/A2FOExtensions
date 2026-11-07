#include "../modules/A2FOLogMailer/report.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}

int main() {
    using namespace log_mailer;
    require(allowed_name("bugreport.txt") && allowed_name("BUGREPORT.TXT"),
            "native report name is case insensitive");
    require(allowed_name("A2FOExtensions.log") && allowed_name("a2fo-x_9.LOG") &&
            allowed_name("a2fo.log"), "extension log names");
    for (const char* name : {"../bugreport.txt", "sub/a2fo.log", "a2fo..log",
                             "a2fo x.log", "a2fo!.log", "other.log", "a2fo.log.txt"})
        require(!allowed_name(name), "unsafe or unrelated filename rejected");
    require(allowed_name("dxwrapper-armadal.log", true) &&
            allowed_name("OTHER.LOG", true) && allowed_name("a2fo.log", true),
            "all-sessions accepts safe game logs");
    for (const char* name : {"../other.log", "sub/other.log", ".hidden.log",
                             "other!.log", "other log", "other.log.txt"})
        require(!allowed_name(name, true), "all-sessions rejects unsafe names");
    require(is_crash(0, true) && is_crash(0xc0000005u, false),
            "fresh native report and exception exit count as crash");
    require(!is_crash(0, false) && !is_crash(1, false) &&
            !is_crash(0xc000013au, false), "normal exit and Ctrl+C excluded");
    require(base64("").empty() && base64("f") == "Zg==" &&
            base64("fo") == "Zm8=" && base64("foo") == "Zm9v" &&
            base64(std::string("\0\xff\x80", 3)) == "AP+A",
            "base64 padding and binary bytes");
    std::puts("log_mailer_test: PASS");
}
