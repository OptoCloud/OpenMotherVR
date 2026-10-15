#pragma once

#include "core/config/settings_store.h"
#include "core/diag/logger.h"
#include "core/input/control_mapper.h"
#include "core/input/control_scheme.h"
#include "core/input/glyphs.h"
#include "core/input/interaction_state.h"
#include "core/input/virtual_pad.h"
#include "core/render/shader_fix_stats.h"
#include "core/stereo/desktop_window.h"
#include "core/stereo/head_clip_delay.h"
#include "core/stereo/mode_policy.h"
#include "core/stereo/reflection_pass.h"
#include "core/stereo/scene_tracker.h"
#include "core/stereo/stereo_aa.h"
#include "core/stereo/stereo_device.h"
#include "core/stereo/stereo_update.h"
#include "core/stereo/virtual_screen_policy.h"
#include "core/ui/commands.h"
#include "core/ui/display_reset.h"
#include "core/ui/settings_push.h"
#include "core/ui/ui_archive.h"
#include "core/vr/runtime_detector.h"
#include "game/addresses.h"
#include "game/input/input_manager.h"
#include "game/layout.h"
#include "game/stereo/follow_camera.h"
#include "game/ui/console.h"
#include "game/ui/script_vars.h"
#include "loader/headset_audio.h"
#include "loader/hook_originals.h"
#include "platform/d3d11/system_d3d11.h"
#include "platform/openvr/runtime_probe.h"
#include "platform/openxr/runtime_probe.h"
#include "platform/openxr/session.h"
#include "platform/openxr/virtual_screen.h"
#include "platform/win32/clock.h"
#include "platform/win32/crash_filter.h"
#include "platform/win32/dialog_thread.h"
#include "platform/win32/file_io.h"
#include "platform/win32/file_log_sink.h"
#include "platform/win32/input/keyboard.h"
#include "platform/win32/payload_files.h"

#include <atomic>

namespace omvr::loader {

// Every long-lived object of the mod, built and wired once at attach. The
// hooks the game, COM and the OS call reach it through context(). Member
// order is construction order; no member does work in its constructor.
struct Context {
    explicit Context(void* module) noexcept;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void* const module; // this DLL's HINSTANCE

    // diag
    platform::win32::SystemClock    clock;
    platform::win32::FileLogSink    log_sink;
    core::diag::Logger              log;
    platform::win32::CrashRecorder  crash;
    // config, ui
    platform::win32::FileStore      files;
    core::config::SettingsStore     settings;
    platform::win32::DialogThread   dialogs;
    platform::win32::ArchiveFiles   archive_files;
    core::ui::UiArchiveDeployer     ui_archive;
    // game
    game::AddressMap                addresses;
    game::StereoManager             manager{};  // the object whose address the game is given
    game::ui::ScriptVars            script_vars;
    game::ui::Console               console;
    core::ui::DisplayReset          display_reset;
    // input
    game::input::InputManager       input_manager;
    core::input::Glyphs             glyphs;
    core::input::ControlSchemes     schemes;
    core::input::VirtualPad         pad;
    core::input::ControlMapper      controls;
    core::input::InteractionState   interactions;
    // ui
    core::ui::SettingsPush          settings_push;
    core::ui::Commands              commands;
    // vr
    platform::openxr::Session       xr;  // shared by the OpenXR runtime and the virtual screen
    platform::openvr::RuntimeProbe  openvr_probe;
    platform::openxr::RuntimeProbe  openxr_probe;
    core::vr::RuntimeDetector       runtimes;
    platform::openxr::VirtualScreen virtual_screen;
    platform::win32::Keyboard       keyboard;
    loader::HeadsetAudio            headset_audio;
    // stereo, render
    game::stereo::FollowCamera      follow;
    core::stereo::SceneTracker      scene;
    core::stereo::ModePolicy        modes;
    core::stereo::StereoUpdate      update;
    core::stereo::StereoDevice      stereo;  // owns the VrRuntime through a factory
    core::stereo::HeadClipDelay     head_clip;
    core::stereo::StereoAa          stereo_aa;
    core::stereo::ReflectionPass    reflections;
    core::stereo::VirtualScreenPolicy screen_policy;
    core::stereo::DesktopWindow     window;
    core::render::ShaderFixStats    shader_fixes;
    // proxy, hooks
    platform::d3d11::SystemD3d11    system_d3d11;
    std::atomic<bool>               ui_set_only{false};
    HookOriginals                   originals;
};

// Builds the context once, in DLL_PROCESS_ATTACH, before the first log line.
// Deliberately leaked: the mod does no work at process exit.
Context& create_context(void* module) noexcept;

// The one accessor. Valid after create_context.
Context& context() noexcept;

} // namespace omvr::loader
