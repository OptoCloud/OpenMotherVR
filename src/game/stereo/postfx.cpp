#include "game/stereo/postfx.h"

#include "game/code_address.h"
#include "game/stereo/field.h"

#include <utility>

namespace omvr::game::stereo {

namespace {

using DrawFn   = void(__cdecl*)();
using PostFxFn = void*(__cdecl*)();
using AaPassFn = void(__fastcall*)(void* postfx, void* edx);
using SmaaFn   = void(__cdecl*)(void* color, void* second, const std::uint8_t* temporal);

} // namespace

void call_fullscreen_draw(const StereoAaTargets& targets) noexcept {
    const DrawFn draw = CodeAddress{targets.fullscreen_draw};
    draw();
}

void* postfx_instance(const StereoAaTargets& targets) noexcept {
    const PostFxFn instance = CodeAddress{targets.postfx};
    return instance();
}

AaMode aa_mode(const void* postfx) noexcept {
    return static_cast<AaMode>(field<std::int32_t>(postfx, kPostFxAaMode));
}

void run_aa_pass_full_screen(const StereoAaTargets& targets, void* postfx) noexcept {
    const AaPassFn aa_pass             = CodeAddress{targets.aa_pass};
    auto& mesh_draw                    = field<std::uint8_t>(postfx, kPostFxMeshDraw);
    const std::uint8_t saved_mesh_draw = std::exchange(mesh_draw, std::uint8_t{0});
    aa_pass(postfx, nullptr);
    mesh_draw = saved_mesh_draw;
}

void call_smaa(const StereoAaTargets& targets, void* color, void* second,
               const std::uint8_t* temporal) noexcept {
    const SmaaFn smaa = CodeAddress{targets.smaa};
    smaa(color, second, temporal);
}

} // namespace omvr::game::stereo
