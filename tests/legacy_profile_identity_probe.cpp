// Map an executable only as data. Never resolve imports or execute entrypoints.
#include "../sdk/include/a2fo_supported_armada.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc!=3)return 2;
    const auto module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!module)return 3;
    const auto actual=static_cast<unsigned>(a2fo::supported_armada::identify(module));
    const auto wanted=static_cast<unsigned>(std::strtoul(argv[2],nullptr,10));
    FreeLibrary(module);
    std::printf("%s identity=%u expected=%u\n",actual==wanted?"PASS":"FAIL",actual,wanted);
    return actual==wanted?0:4;
}
