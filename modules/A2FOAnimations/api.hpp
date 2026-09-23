#pragma once
// Optional bridge used by AnimatedHardpoints; process-lifetime DLL exports.
// true means the matrix was evaluated by the per-instance clip controller.
using A2FO_AnimationsEvaluateFn = bool (__cdecl*)(void* instance, void* channel, void* fallback_target);
// true means the instance has an active/held controlled clip; bypass old caches.
using A2FO_AnimationsControlledFn = bool (__cdecl*)(void* instance);

// Notification from the completed projectile-build observer. No gameplay claim.
using A2FO_AnimationsWeaponFiredFn = void (__cdecl*)(void* weapon);
