// Namespace the original generic ABI helpers inside the consolidated image.
#define a2fo_call_thiscall_1 a2fo_builtin_hybrid_call_thiscall_1
#define a2fo_call_thiscall_2 a2fo_builtin_hybrid_call_thiscall_2
// The original standalone TU contains one unused diagnostic latch.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "../modules/A2FOHybridBuild/hybrid_production_runtime.cpp"
#pragma GCC diagnostic pop
