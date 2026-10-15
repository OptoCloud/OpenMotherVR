#include "game/stereo/postfx.h"

namespace omvr::game::stereo {

namespace {

// R: stereo (A3): a member of the object that owns the fix.
StereoAaTargets g_aa{};

} // namespace

void install_stereo_aa(const StereoAaTargets& targets) noexcept { g_aa = targets; }

void call_fullscreen_draw() noexcept {
    using DrawFn = void (__cdecl*)();
    reinterpret_cast<DrawFn>(g_aa.fullscreen_draw)();
}

void* postfx_instance() noexcept {
    using PostFxFn = void* (__cdecl*)();
    return reinterpret_cast<PostFxFn>(g_aa.postfx)();
}

void call_aa_pass(void* postfx) noexcept {
    using AaFn = void (__fastcall*)(void*, void*);
    reinterpret_cast<AaFn>(g_aa.aa_pass)(postfx, nullptr);
}

void call_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept {
    using SmaaFn = void (__cdecl*)(void*, void*, const std::uint8_t*);
    reinterpret_cast<SmaaFn>(g_aa.smaa)(color, second, temporal);
}

} // namespace omvr::game::stereo
