#include "lua_host.hpp"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
std::vector<std::string> messages;

void require(bool condition) {
    if (!condition) std::abort();
}

void log_line(const std::string& message) {
    messages.push_back(message);
}

void write_text(const std::string& path, const char* text) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    const DWORD size = static_cast<DWORD>(std::strlen(text));
    require(WriteFile(file, text, size, &written, nullptr) != FALSE);
    require(written == size);
    CloseHandle(file);
}
}

int main() {
    const std::string root = "lua_host_smoke_root";
    CreateDirectoryA(root.c_str(), nullptr);
    CreateDirectoryA((root + "\\scripts").c_str(), nullptr);
    write_text(root + "\\scripts\\01_good.lua",
        "a2fo.require_api(1, 1)\n"
        "assert(a2fo.has_capability('declared_destroyed_odf_fields'))\n"
        "a2fo.on_object_destroyed({'Foo', 'bar'}, function() return nil end)\n");
    write_text(root + "\\scripts\\02_bad.lua",
        "a2fo.on_object_destroyed({'must_not_leak'}, function() return nil end)\n"
        "error('intentional rollback')\n");

    a2fo::LuaHost host;
    a2fo::LuaEngineApi engine;
    const bool initialized = a2fo::initialize_lua_host(
        {root}, host, &log_line, engine);
    require(!initialized);
    require(host.loaded_script_count == 1);
    require(host.object_destroyed_callbacks.size() == 1);
    const std::vector<std::string> fields =
        a2fo::object_destroyed_odf_fields(host);
    require(std::find(fields.begin(), fields.end(), "basename") != fields.end());
    require(std::find(fields.begin(), fields.end(), "foo") != fields.end());
    require(std::find(fields.begin(), fields.end(), "bar") != fields.end());
    require(std::find(fields.begin(), fields.end(), "must_not_leak") ==
            fields.end());
}
