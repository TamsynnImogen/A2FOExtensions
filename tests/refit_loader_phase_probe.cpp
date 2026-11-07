#include <windows.h>

// A task-owned DLL that invokes the host fixture while Windows still holds
// its loader lock for DLL_PROCESS_ATTACH. No game modules or GUI are loaded.
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    const auto image = GetModuleHandleA(nullptr);
    const auto address = image ? GetProcAddress(image, "refit_loader_probe_callback") : nullptr;
    if (!address)
      return FALSE;
    using Callback = void (*)();
    reinterpret_cast<Callback>(address)();
  }
  return TRUE;
}
