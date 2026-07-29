#include <windows.h>

#include <iostream>

int main() {
    HMODULE module = LoadLibraryA("A2FOExtensions.dll");
    if (!module) {
        std::cerr << "LoadLibrary failed: " << GetLastError() << '\n';
        return 1;
    }
    Sleep(1500);
    FreeLibrary(module);
    return 0;
}
