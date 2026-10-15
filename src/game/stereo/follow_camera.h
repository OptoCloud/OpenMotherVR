#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

// The game's first-person camera behaviour (FollowBehavior) and the field
// access the stereo update needs.
namespace omvr::game::stereo {

inline constexpr std::ptrdiff_t kFollowYaw         = 0x204;  // ±1 = ±180°
inline constexpr std::ptrdiff_t kFollowYawMirror   = 0x1FC;
inline constexpr std::ptrdiff_t kFollowLookEnabled = 0x264;  // game stick/mouse look
inline constexpr std::ptrdiff_t kFollowRootPos     = 0x1C0;  // character root, world x y z
inline constexpr std::ptrdiff_t kFollowHeadPos     = 0x1CC;  // "Head1" bone, world x y z

template <class T = void>
T* field_ptr(std::uint8_t* base, std::ptrdiff_t off) noexcept {
    return *reinterpret_cast<T**>(base + off);
}

template <class T>
T& field(void* base, std::ptrdiff_t off) noexcept {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + off);
}

// The active first-person camera (FollowBehavior), null while none is
// active: the object may be freed once deactivated (a level unload).
// Original: DAT_1006863c, DAT_10068638, which keep the last one. Game thread.
struct FollowCamera {
    std::uint8_t* g_follow = nullptr;
    bool          g_follow_active = false;

    // Debug: the camera around a snap step, to tell the body catching up
    // with the new heading (the eye follows the head bone, which swings
    // around the root as the body turns) from game look turning as well.
    void trace_snap(std::chrono::steady_clock::time_point step, bool keyboard_mouse) noexcept;
};

} // namespace omvr::game::stereo
