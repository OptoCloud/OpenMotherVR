// The AI.exe address table: every named slot inside the 127 rows, no two
// names on one row, every named 2015 row known, the recon rows and inline
// sites as they were before the restructure (1d908cf).

#include "game/addresses.h"
#include "game/x86.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <utility>

namespace {

namespace table {
#include "game/ai_address_tables.inc"
} // namespace table

using omvr::game::AiAddr;

// Every enumerator of AiAddr.
constexpr std::array kNamedSlots = {
    AiAddr::FollowOnDeactivate,
    AiAddr::FollowOnActivatePatch,
    AiAddr::FollowLookState,
    AiAddr::CameraNoiseCall,
    AiAddr::HeadClipCompare,
    AiAddr::CameraCorrectionBranch,
    AiAddr::FireStr2,
    AiAddr::FireBool,
    AiAddr::FireInt,
    AiAddr::FireFloat,
    AiAddr::AttractOpen,
    AiAddr::PauseMenuClose,
    AiAddr::LoadingScreenClose,
    AiAddr::MenuOpen,
    AiAddr::MenuClose,
    AiAddr::OriginalCmdHandler,
    AiAddr::BootupMoviePush,
    AiAddr::UiMouseWidthLoad,
    AiAddr::UiMouseHeightLoad,
    AiAddr::UiInputModeBranch,
    AiAddr::PauseMenuOpen,
    AiAddr::LoadingScreenOpen,
    AiAddr::UiEntityOnMessage,
    AiAddr::CutsceneVisibilityStart,
    AiAddr::CutsceneVisibilityFinish,
    AiAddr::LevelFadeStart,
    AiAddr::LevelFadeFinish,
    AiAddr::SsaoSettingBranch,
    AiAddr::PadIsDown,
    AiAddr::PadGetValue,
    AiAddr::PadGetAxis,
    AiAddr::PadControlToName,
    AiAddr::PadHasActivity,
    AiAddr::KeyboardKeyDown,
    AiAddr::KeyboardAnyKey,
    AiAddr::GraphicsSetOption,
    AiAddr::StereoManagerAlloc,
    AiAddr::HeadPoseCallA,
    AiAddr::HeadPoseCallB,
    AiAddr::GameDeviceEyeTransform,
    AiAddr::GameDeviceFovAspect,
    AiAddr::GameDeviceEyeScaleOffset,
    AiAddr::BuildUiQuad,
    AiAddr::GamePushMode,
    AiAddr::GamePopMode,
    AiAddr::RendererDevice,
    AiAddr::RendererSwapChain,
    AiAddr::RendererContext,
    AiAddr::RendererRtTexture,
    AiAddr::RendererRtSrv,
    AiAddr::AudioReopenOutput,
    AiAddr::MouseActive,
    AiAddr::FollowVtable,
    AiAddr::UiEntityMessageSlot,
    AiAddr::UiPakPathPtr,
    AiAddr::DisplayResetFlag,
    AiAddr::GraphicsSettings,
    AiAddr::InputManagerPtr,
    AiAddr::PovManagerPtr,
    AiAddr::ExitRequest,
    AiAddr::RendererPtr,
    AiAddr::CreateDeviceCall,
    AiAddr::StereoUpdateCall,
    AiAddr::VirtualScreenCall,
    AiAddr::HudPanelCall,
    AiAddr::HmdResolutionCall,
    AiAddr::RenderTargetSizeCall,
    AiAddr::PushModeCallA,
    AiAddr::PushModeCallB,
    AiAddr::PushModeCallC,
    AiAddr::PopModeCallA,
    AiAddr::PopModeCallB,
    AiAddr::PopModeCallC,
    AiAddr::ResumeGamePopMode,
    AiAddr::PauseMenuPushMode,
    AiAddr::MenuVtable,
    AiAddr::PauseMenuVtable,
    AiAddr::LoadingScreenVtable,
    AiAddr::AttractScreenVtable,
    AiAddr::LoaderSleepPush,
    AiAddr::PadVtable,
    AiAddr::PadConnectedSet,
    AiAddr::LeftStickPromptCall,
    AiAddr::CmdDispatchSlot,
};

consteval bool all_named_slots_in_table() {
    return std::ranges::all_of(
        kNamedSlots, [](AiAddr a) { return std::to_underlying(a) < omvr::game::kAddressTableRows; });
}
static_assert(all_named_slots_in_table());

consteval bool no_two_names_on_one_row() {
    std::array<bool, omvr::game::kAddressTableRows> used{};
    for (const AiAddr a : kNamedSlots) {
        if (used[std::to_underlying(a)]) return false;
        used[std::to_underlying(a)] = true;
    }
    return true;
}
static_assert(no_two_names_on_one_row());

static_assert(table::kAiRvas2015.size() == omvr::game::kAddressTableRows);
static_assert(table::kAiRvas2020.size() == omvr::game::kAddressTableRows);

TEST_CASE("every named slot has a 2015 address") {
    for (const AiAddr a : kNamedSlots) {
        CAPTURE(std::to_underlying(a));
        CHECK(table::kAiRvas2015[std::to_underlying(a)] != 0u);
    }
}

TEST_CASE("the recon rows: all known on 2015, none on 2020") {
    using omvr::game::ReconRvas;
    constexpr std::size_t kRows = sizeof(ReconRvas) / sizeof(std::uint32_t);
    const auto r2015            = std::bit_cast<std::array<std::uint32_t, kRows>>(omvr::game::kReconRvas2015);
    const auto r2020            = std::bit_cast<std::array<std::uint32_t, kRows>>(omvr::game::kReconRvas2020);
    CHECK(std::ranges::none_of(r2015, [](std::uint32_t v) { return v == 0; }));
    CHECK(std::ranges::all_of(r2020, [](std::uint32_t v) { return v == 0; }));
    CHECK(omvr::game::kReconRvas2015.reflection_render_call == 0x005B5D91u);
    CHECK(omvr::game::kReconRvas2015.mode_stack_append == 0x00294540u);
}

TEST_CASE("the inline sites are the values before the restructure") {
    const auto& s15 = omvr::game::kInlineSites2015;
    CHECK(s15.stereo_mode_tests == std::array<std::uint32_t, 8>{0x000336B6, 0x0060FD58, 0x0060FE50,
                                                                0x0060FEBD, 0x0060FF15, 0x00611740,
                                                                0x0061378A, 0x00613C80});
    CHECK(s15.left_stick_prompt_call_2 == 0x0042D18Cu);
    CHECK(s15.ui_mouse_width_disp_lo == 0xDC);
    CHECK(s15.ui_mouse_height_disp_lo == 0xE0);

    const auto& s20 = omvr::game::kInlineSites2020;
    CHECK(s20.stereo_mode_tests == std::array<std::uint32_t, 8>{0x00042F96, 0x00562238, 0x00562330,
                                                                0x0056239D, 0x005623F5, 0x00563C10,
                                                                0x00565C5A, 0x00566150});
    CHECK(s20.left_stick_prompt_call_2 == 0x0043E2DCu);
    CHECK(s20.ui_mouse_width_disp_lo == 0x8C);
    CHECK(s20.ui_mouse_height_disp_lo == 0x90);
}

TEST_CASE("a per-build table picks the running build's rows") {
    using omvr::game::GameBuild;
    CHECK(&omvr::game::kReconRvas[GameBuild::Original2015] == &omvr::game::kReconRvas.original_2015);
    CHECK(&omvr::game::kReconRvas[GameBuild::Patched2020] == &omvr::game::kReconRvas.patched_2020);
    CHECK(omvr::game::kReconRvas[GameBuild::Original2015].reflection_render_call == 0x005B5D91u);
    CHECK(omvr::game::kReconRvas[GameBuild::Patched2020].reflection_render_call == 0u);
    CHECK(omvr::game::kInlineSites[GameBuild::Patched2020].left_stick_prompt_call_2 == 0x0043E2DCu);
    CHECK(omvr::game::kInlineSites[GameBuild::Original2015].left_stick_prompt_call_2 == 0x0042D18Cu);
}

// The melee kill start's first bytes in the 2015 AI.exe (RVA 0x0026DC20):
// `push -1; push 0x01126A78; mov eax, fs:[0]`, the pushed VA being its
// exception handler at RVA 0x00D26A78 with the file's base 0x400000.
TEST_CASE("an exception-frame prologue is expected at this image's handler address") {
    constexpr std::array<std::uint8_t, 12> kTemplate = {0x6A, 0xFF, 0x68, 0x00, 0x00, 0x00,
                                                        0x00, 0x64, 0xA1, 0x00, 0x00, 0x00};
    constexpr std::uint32_t kHandler                 = omvr::game::kActionRvas2015.melee_kill_eh_handler;
    CHECK(
        omvr::game::x86::with_imm32<3>(kTemplate, 0x00400000u + kHandler) ==
        std::array<std::uint8_t, 12>{0x6A, 0xFF, 0x68, 0x78, 0x6A, 0x12, 0x01, 0x64, 0xA1, 0x00, 0x00, 0x00});
    // Rebased to 0x00AC0000, as logged on a real launch: the loader moves
    // the operand by the same delta.
    CHECK(
        omvr::game::x86::with_imm32<3>(kTemplate, 0x00AC0000u + kHandler) ==
        std::array<std::uint8_t, 12>{0x6A, 0xFF, 0x68, 0x78, 0x6A, 0x7E, 0x01, 0x64, 0xA1, 0x00, 0x00, 0x00});
    CHECK(omvr::game::kActionRvas2015.play_request_eh_handler == 0x00D226E0u);
    CHECK(omvr::game::kActionRvas2015.play_counterpart_eh_handler == 0x00D210A8u);
}

} // namespace
