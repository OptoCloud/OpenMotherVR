// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings_store.h"
#include "core/diag/crash_safe_sink.h"
#include "core/diag/flush_schedule.h"
#include "core/diag/logger.h"
#include "core/diag/thread_roles.h"
#include "core/hands/body_controls.h"
#include "core/hands/body_hider.h"
#include "core/hands/hand_back.h"
#include "core/hands/hands.h"
#include "core/hands/haptics.h"
#include "core/hands/real_crouch.h"
#include "core/input/control_mapper.h"
#include "core/input/control_scheme.h"
#include "core/input/glyphs.h"
#include "core/input/interaction_state.h"
#include "core/input/virtual_pad.h"
#include "core/render/alpha_light.h"
#include "core/render/constant_smoke.h"
#include "core/render/post_fx.h"
#include "core/render/shader_fix_stats.h"
#include "core/stereo/comfort_view.h"
#include "core/stereo/desktop_window.h"
#include "core/stereo/head_clip_delay.h"
#include "core/stereo/head_clip_pivot.h"
#include "core/stereo/height_calibration.h"
#include "core/stereo/play_space.h"
#include "core/stereo/visibility_trace.h"
#include "core/stereo/mode_policy.h"
#include "core/stereo/reflection_pass.h"
#include "core/stereo/scene_tracker.h"
#include "core/stereo/stereo_aa.h"
#include "core/stereo/stereo_device.h"
#include "core/stereo/stereo_update.h"
#include "core/stereo/virtual_screen_policy.h"
#include "core/stereo/vr_off_hotkey.h"
#include "core/ui/camera_takeovers.h"
#include "core/ui/commands.h"
#include "core/ui/display_reset.h"
#include "core/ui/play_state.h"
#include "core/ui/settings_push.h"
#include "core/ui/ui_archive.h"
#include "core/ui/crosshair_gate.h"
#include "core/ui/hud_placement.h"
#include "core/ui/hud_toggle.h"
#include "core/ui/hud_visibility.h"
#include "core/vr/runtime_detector.h"
#include "core/vr/runtime_failure.h"
#include "game/addresses.h"
#include "game/input/input_manager.h"
#include "game/layout.h"
#include "game/sites/camera_takeovers.h"
#include "game/stereo/follow_camera.h"
#include "game/stereo/post_fx_effects.h"
#include "game/ui/console.h"
#include "game/ui/script_vars.h"
#include "game/ui/hud_objects.h"
#include "loader/gesture_context.h"
#include "loader/headset_audio.h"
#include "loader/hook_targets.h"
#include "loader/launch_options.h"
#include "loader/play_space_control.h"
#include "loader/runtime_state.h"
#include "loader/stick_context.h"
#include "platform/d3d11/render/mirror_renderer.h"
#include "platform/d3d11/render/vignette_renderer.h"
#include "platform/d3d11/system_d3d11.h"
#include "platform/discord/presence.h"
#include "platform/openvr/runtime_probe.h"
#include "platform/openxr/runtime_probe.h"
#include "platform/openxr/session.h"
#include "platform/openxr/virtual_screen.h"
#include "platform/win32/clock.h"
#include "platform/win32/crash_filter.h"
#include "platform/win32/current_thread.h"
#include "platform/win32/dialog_thread.h"
#include "platform/win32/file_io.h"
#include "platform/win32/file_log_sink.h"
#include "platform/win32/input/keyboard.h"
#include "platform/win32/log_flusher.h"
#include "platform/win32/payload_files.h"

#include <atomic>

namespace omvr::loader {

// Every long-lived object of the mod, built and wired once at attach. The
// hooks the game, COM and the OS call reach it through context(). Member
// order is construction order; no member does work in its constructor. A
// member may take a reference to one declared after it, as long as it does
// not use it before attach.
struct Context {
    explicit Context(void* module) noexcept;
    Context(const Context&)            = delete;
    Context& operator=(const Context&) = delete;

    void* const module; // this DLL's HINSTANCE

    // diag
    platform::win32::SystemClock clock;
    platform::win32::FileLogSink log_sink;
    platform::win32::CurrentThread current_thread;
    core::diag::ThreadRoles thread_roles;      // which thread is main, render, mod (11 §1.4)
    core::diag::FlushScheduler flush_schedule; // the mod thread's pace (22 §14)
    platform::win32::LogFlusher log_flusher;   // Controls.CrashSafeLog's mod thread (22 §14)
    core::diag::CrashSafeSink log_out;         // the logger's sink: the file, flushed by policy
    core::diag::Logger log;
    platform::win32::CrashRecorder crash;
    // config, ui
    ProcessLaunchSwitches launch; // the command-line switches, read at attach
    platform::win32::FileStore files;
    core::config::SettingsStore settings;
    platform::win32::DialogThread dialogs;
    platform::win32::ArchiveFiles archive_files;
    core::ui::UiArchiveDeployer ui_archive;
    // game
    game::AddressMap addresses;
    game::StereoManager manager{}; // the object whose address the game is given
    game::ui::ScriptVars script_vars;
    game::ui::Console console;
    RuntimeStateView runtime_state; // over `runtimes` and `stereo`, declared below
    // The standing height calibration and -vrscale, for the options pages;
    // over `calibration`, declared below.
    PlaySpaceControlView play_space_control;
    core::ui::DisplayReset display_reset;
    // input
    game::input::InputManager input_manager;
    core::input::Glyphs glyphs;
    core::input::ControlSchemes schemes;
    core::input::VirtualPad pad;
    // Where the left stick is free for the d-pad (22 §7); over `manager`
    // and `interactions`, declared below.
    StickContextView stick_context;
    // The body gestures (22 §8.2, §8.4, §9) over the same state.
    GestureContextView gesture_context;
    core::input::Gestures gestures;
    core::input::ControlMapper controls;
    core::input::InteractionState interactions;
    // hands (21): the tracked hands, aim from the hand, the tracker in hand
    core::hands::Hands hands;
    // haptics (21 §14): the game's feedback on the controllers, game
    // threads to the render thread
    core::hands::Haptics haptics;
    // The HUD options (23 §7, 22 §8.3): the player's HUD state (game
    // thread; the gesture's toggle from the render thread), the crosshair
    // gate (the HUD update), the panel placement (the overlay draw), the
    // toggle gesture in front of `controls` (render thread), and the game
    // functions their hooks call.
    core::ui::HudVisibility hud_visibility;
    core::ui::CrosshairGate crosshair;
    core::ui::HudPlacer hud_placer;
    core::ui::HudToggleSink hud_toggle;
    game::ui::HudTargets hud_targets{};
    // Interaction.CameraTakeovers (23 §4): the rule its replacements ask,
    // and the game functions they call when a takeover is kept. Game thread.
    core::ui::CameraTakeoverRule camera_takeovers;
    game::sites::CameraTakeoverTargets camera_targets{};
    // body (21 §10 to §12): body visibility (game thread to the final-pose
    // jobs), real crouch and the control hand-back (game thread), and real
    // crouch's presses in front of `hud_toggle` (render thread).
    core::hands::BodyHider body_hider;
    core::hands::RealCrouch real_crouch;
    core::hands::HandBack hand_back;
    core::hands::BodyControls body_controls;
    // ui
    core::ui::SettingsPush settings_push;
    core::ui::Commands commands;
    // vr
    core::vr::RuntimeFailure failure; // why the runtime of this run failed to start
    platform::openxr::Session xr;     // shared by the OpenXR runtime and the virtual screen
    platform::openvr::RuntimeProbe openvr_probe;
    platform::openxr::RuntimeProbe openxr_probe;
    core::vr::RuntimeDetector runtimes;
    platform::openxr::VirtualScreen virtual_screen;
    platform::win32::Keyboard keyboard;
    loader::HeadsetAudio headset_audio;
    // stereo, render
    game::stereo::FollowCamera follow;
    core::stereo::SceneTracker scene;
    core::stereo::ModePolicy modes;
    core::stereo::StereoUpdate update;
    // Controls.VrOffHotkey's hold (22 §13). Game thread.
    core::stereo::VrOffHotkey vr_off;
    core::stereo::StereoDevice stereo;      // owns the VrRuntime through a factory
    platform::d3d11::MirrorRenderer mirror; // the desktop mirror on the stereo device's objects
    // The comfort vignette and snap fade, game to render thread (22 §15), and
    // their pass in slot 6 (22 §5).
    core::stereo::ComfortView comfort;
    platform::d3d11::VignetteRenderer vignette;
    core::stereo::HeadClipDelay head_clip;
    core::stereo::HeadClipPivot head_clip_pivot;
    // Seated and standing play, the eye's place and the world scale (22 §6,
    // 21 §11), and the standing height calibration (22 §6.2). Game thread.
    core::stereo::PlaySpace play_space;
    core::stereo::HeightCalibration calibration;
    // Dev. Debug Options: what darkens the view (the game's head-in-wall
    // fade, the scene visibility the stereo update finds, and what it
    // leaves). Game thread.
    core::stereo::VisibilityTrace head_clip_trace;
    core::stereo::VisibilityTrace found_visibility_trace;
    core::stereo::VisibilityTrace final_visibility_trace;
    core::stereo::StereoAa stereo_aa;
    core::stereo::ReflectionPass reflections;
    core::stereo::VirtualScreenPolicy screen_policy;
    core::stereo::DesktopWindow window;
    core::render::ShaderFixStats shader_fixes;
    // The upright-sprite fix of the vertex shaders (24 §14).
    core::render::ShaderFixStats sprite_fixes;
    // Constant smoke and fog lighting of the pixel shaders (24 §15).
    core::render::ConstantSmoke constant_smoke;
    // Dev. Debug Options: the particle lighting lookup's correction, logged
    // once per change (dumps/13). Render thread.
    core::render::AlphaLightTrace alpha_light_trace;
    // The 2D-only effect toggles (24 §8): their keys, main to render
    // thread, and the game functions their replacements call.
    core::render::PostFxEffects post_fx;
    game::stereo::PostFxEffectTargets post_fx_targets{};
    // proxy, hooks
    platform::d3d11::SystemD3d11 system_d3d11;
    std::atomic<bool> ui_set_only{false};
    HookTargets targets;
    // What the player is doing (level, menu, cutscene), and the Discord
    // Rich Presence whose worker polls it.
    core::ui::PlayState play;
    platform::discord::Presence discord;
};

// Builds the context once, in DLL_PROCESS_ATTACH, before the first log line.
// Deliberately leaked: the mod does no work at process exit.
Context& create_context(void* module) noexcept;

// The one accessor. Valid after create_context.
Context& context() noexcept;

} // namespace omvr::loader
