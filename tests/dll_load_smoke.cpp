#include <windows.h>

int main() {
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    if (!core || !GetProcAddress(core, "A2FO_Initialize") ||
        !GetProcAddress(core, "A2FO_NebulaRendererStatus") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterEmissiveClass") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterEmissiveMaterials") ||
        !GetProcAddress(core, "A2FO_NebulaBeginCraftRender") ||
        !GetProcAddress(core, "A2FO_NebulaEndCraftRender")) return 1;

    HMODULE nebula = LoadLibraryA("modules\\A2FONebulaRenderer.dll");
    if (!nebula || !GetProcAddress(nebula, "A2FO_ModuleInit") ||
        !GetProcAddress(nebula, "A2FO_ModuleShutdown") ||
        !GetProcAddress(nebula, "A2FONebulaRenderer_RegisterClass")) {
        return 2;
    }

    // The proxy intentionally refuses to attach without the shipped renamed
    // Win2kDisableTaskSwitch.original.dll. Its exports are checked by
    // `make verify`; this standalone smoke test covers the self-contained core.
}
