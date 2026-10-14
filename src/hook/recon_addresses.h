#pragma once

#include "integration/exe_check.h"

#include <array>
#include <cstdint>

// AI.exe addresses the reconstruction uses that the original's table does
// not have: ReconRvas for fixes the original never made (RVAs per build; 0
// means not located for that build, and the fix depending on it stays off),
// and InlineSites for patches the original codes inline.

namespace mothervr::hook {

struct ReconRvas {
    // Planar reflections (dumps/07_reflections_head_pose.md).
    std::uint32_t reflection_render_call;  // `call ReflectionPOV render` in POVManager::RenderPOVs
    std::uint32_t reflection_render;       // ReflectionPOV render (thiscall, no args)
    std::uint32_t apply_eye_transform;     // Stereo::ApplyEyeTransform(eye, view*, proj*) (thiscall, ret 0xC)
    std::uint32_t pov_update_frustum;      // POV frustum from its view/proj (thiscall, no args)
    std::uint32_t renderer_eye_owner;      // global pointer; its +0x2C is the current eye
    std::uint32_t clear_render_target;     // clear of the bound target (cdecl); 6-byte prologue

    // Anti-aliasing in stereo.
    std::uint32_t stereo_output_draw_call; // `call FullscreenDraw` in the final post chain's stereo path
    std::uint32_t fullscreen_draw;         // full-screen triangle draw (cdecl, no args)
    std::uint32_t postfx_instance;         // PostFX getter (cdecl, no args)
    std::uint32_t aa_pass;                 // PostFX AA pass (thiscall, no args)
    std::uint32_t smaa_call;               // `call SMAA` in the AA pass
    std::uint32_t smaa;                    // SMAA runner (cdecl, 3 args)

    // Object culling: two
    // `call rel32` in the per-object visibility loop and their callees, both
    // __cdecl with 3 args and the result in al. The loop's third optional
    // test (the shadow casters' main-view volume, 0x619920) is left alone.
    std::uint32_t cull_distance_call;      // distance/LOD test, the loop's only caller
    std::uint32_t cull_distance;
    std::uint32_t cull_occlusion_call;     // occlusion-buffer test, main POV only
    std::uint32_t cull_occlusion;

    // Head-in-wall fade (dumps/06c_detours_and_patches.md bdc+3): the one
    // `call rel32` of HeadClipFade (__stdcall, 2 args, float in ST0), whose
    // result the caller stores as the scene visibility.
    std::uint32_t head_clip_fade_call;
    std::uint32_t head_clip_fade;

    // Script event tap: a retail
    // no-op, `mov eax, ecx; ret 4`, that scripts pass debug strings to.
    std::uint32_t event_tap;
};

inline constexpr ReconRvas kReconRvas2015 = {
    .reflection_render_call  = 0x005B5D91,
    .reflection_render       = 0x005B5C40,
    .apply_eye_transform     = 0x005B86A0,
    .pov_update_frustum      = 0x005B2FB0,
    .renderer_eye_owner      = 0x013583E4,
    .clear_render_target     = 0x00621770,
    .stereo_output_draw_call = 0x006101DC,
    .fullscreen_draw         = 0x005A0980,
    .postfx_instance         = 0x005F19C0,
    .aa_pass                 = 0x00600210,
    .smaa_call               = 0x00600479,
    .smaa                    = 0x0061A4C0,
    .cull_distance_call      = 0x0061FD95,
    .cull_distance           = 0x00619A50,
    .cull_occlusion_call     = 0x0061FDB9,
    .cull_occlusion          = 0x00619860,
    .head_clip_fade_call     = 0x0034E2C1,
    .head_clip_fade          = 0x00346DE0,
    .event_tap               = 0x00A1BCC0,
};

inline constexpr ReconRvas kReconRvas2020 = {};

inline const ReconRvas& recon_rvas(integration::AIVersion version) noexcept {
    return version == integration::AIVersion::Patched_2020 ? kReconRvas2020 : kReconRvas2015;
}

// Sites and bytes the original writes but codes inline in FUN_1001d6f0
// instead of keeping them in its address table.
struct InlineSites {
    // The eight `cmp reg, eax` "Stereo Mode == Rift" tests (listing lines
    // 72..100: two RVA arrays on the installer's stack).
    std::array<std::uint32_t, 8> stereo_mode_tests;
    // The second `call IsMoveOnLeftStick`, which the original addresses as
    // DAT_10066db8 + 0xEC (dumps/06c_detours_and_patches.md, db8).
    std::uint32_t left_stick_prompt_call_2;
    // Low byte of the disp32 at UiMouseWidthLoad/UiMouseHeightLoad + 4: the
    // 1280x720 UI reference size, 0x10 bytes past the backbuffer size.
    std::uint8_t ui_mouse_width_disp_lo;
    std::uint8_t ui_mouse_height_disp_lo;
};

inline constexpr InlineSites kInlineSites2015 = {
    .stereo_mode_tests = {0x000336B6, 0x0060FD58, 0x0060FE50, 0x0060FEBD,
                          0x0060FF15, 0x00611740, 0x0061378A, 0x00613C80},
    .left_stick_prompt_call_2 = 0x0042D18C,
    .ui_mouse_width_disp_lo   = 0xEC,
    .ui_mouse_height_disp_lo  = 0xF0,
};

inline constexpr InlineSites kInlineSites2020 = {
    .stereo_mode_tests = {0x00042F96, 0x00562238, 0x00562330, 0x0056239D,
                          0x005623F5, 0x00563C10, 0x00565C5A, 0x00566150},
    .left_stick_prompt_call_2 = 0x0043E2DC,
    .ui_mouse_width_disp_lo   = 0x9C,
    .ui_mouse_height_disp_lo  = 0xA0,
};

inline const InlineSites& inline_sites(integration::AIVersion version) noexcept {
    return version == integration::AIVersion::Patched_2020 ? kInlineSites2020 : kInlineSites2015;
}

} // namespace mothervr::hook
