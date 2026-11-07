#pragma once

namespace a2fo::collective_xp {

// Optional native module ABI. A null result rejects this admission.
#ifdef _WIN32
using ResolveBuildClassFn = void* (__cdecl*)(void* producer, void* base_class);
#else
using ResolveBuildClassFn = void* (*)(void* producer, void* base_class);
#endif

// A missing module leaves the existing queue target unchanged.
inline void* resolve_queue_class(void* producer, void* base_class,
                                 ResolveBuildClassFn resolver) noexcept {
    return resolver ? resolver(producer, base_class) : base_class;
}

}  // namespace a2fo::collective_xp
