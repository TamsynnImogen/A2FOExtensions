/*
 * Small COM ownership helpers used where a borrowed engine pointer must be
 * retained across renderer lifetime transitions.
 */

#pragma once

namespace a2fo {

template <typename Interface, typename BeforeRelease>
bool adopt_com_owner(Interface*& owner, Interface* candidate,
                     BeforeRelease before_release) noexcept {
    if (!candidate || owner == candidate) return false;

    // Acquire the replacement first. The caller may otherwise be observing
    // the final borrowed reference supplied by the engine.
    candidate->AddRef();
    Interface* previous = owner;
    if (previous) before_release(previous);
    owner = candidate;
    if (previous) previous->Release();
    return true;
}

template <typename Interface, typename BeforeRelease>
void release_com_owner(Interface*& owner,
                       BeforeRelease before_release) noexcept {
    Interface* previous = owner;
    if (!previous) return;
    before_release(previous);
    owner = nullptr;
    previous->Release();
}

template <typename Interface, typename BeforeRelease>
bool release_matching_com_owner(Interface*& owner, Interface* candidate,
                                BeforeRelease before_release) noexcept {
    if (!candidate || owner != candidate) return false;
    release_com_owner(owner, before_release);
    return true;
}

}  // namespace a2fo
