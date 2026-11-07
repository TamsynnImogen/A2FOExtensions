#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::animations
#define A2FO_ModuleInit A2FOBuiltinAnimations_Init
#define A2FO_ModuleShutdown A2FOBuiltinAnimations_Shutdown
#include "../modules/A2FOAnimations/module.cpp"
#include "builtin_animation_observation.hpp"

bool a2fo::builtin_animations_native_playback(HMODULE image,animations::NativePlaybackProof& output) noexcept {
    return g_native_coverage.snapshot(image,output);
}

extern "C" bool A2FOBuiltinAnimations_Ready() noexcept {
    return g_ready;
}
