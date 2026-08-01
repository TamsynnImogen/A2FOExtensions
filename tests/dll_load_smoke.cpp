#include <windows.h>

int main() {
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    if (!core || !GetProcAddress(core, "A2FO_Initialize")) return 1;

    // The proxy intentionally refuses to attach without the shipped renamed
    // Win2kDisableTaskSwitch.original.dll. Its exports are checked by
    // `make verify`; this standalone smoke test covers the self-contained core.
}
