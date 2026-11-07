#include <windows.h>

extern "C" __declspec(dllexport) int A2FOResourceProbe() { return 1; }

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
