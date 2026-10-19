// core/ui: every mvr_* command rule, the command runner's order (save, then
// quit), the display reset, the settings push, the runtime menu, PAK2 and
// the UI patch, the camera takeover rule.

#include "core/config/settings_store.h"
#include "core/config/tuning.h"
#include "core/input/control_scheme.h"
#include "core/input/glyphs.h"
#include "core/ui/camera_takeovers.h"
#include "core/ui/commands.h"
#include "core/ui/compat_messages.h"
#include "core/ui/display_reset.h"
#include "core/ui/pak2.h"
#include "core/ui/runtime_menu.h"
#include "core/ui/settings_push.h"
#include "core/ui/ui_contract.h"
#include "core/ui/ui_patch.h"
#include "core/vr/runtime_failure.h"
#include "fakes/fakes.h"
#include "util/hash/crc32.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace omvr::core::ui;
using omvr::core::config::BodyVisible;
using omvr::core::config::CommandEffect;
using omvr::core::config::FlashlightAim;
using omvr::core::config::MirrorViewType;
using omvr::core::config::PlayMode;
using omvr::core::config::Settings;
using omvr::core::config::VRRuntime;
using omvr::tests::bytes_of;
using omvr::tests::FakeArchiveFiles;
using omvr::tests::FakeGameSignals;
using omvr::tests::FakePlaySpaceControl;
using omvr::tests::FakeRuntimeState;
using omvr::tests::MemoryFileStore;
using omvr::tests::RecordingLog;
using omvr::tests::RecordingScriptVariables;
using omvr::tests::string_of;
using Kind   = CommandOutcome::Kind;
using Chosen = omvr::core::vr::DetectResult::Chosen;

TEST_CASE("is_mod_command: the mvr_ prefix after leading spaces") {
    CHECK(is_mod_command("mvr_debug 1"));
    CHECK(is_mod_command("   mvr_debug 1"));
    CHECK_FALSE(is_mod_command("quit"));
    CHECK_FALSE(is_mod_command(""));
    CHECK_FALSE(is_mod_command("   "));
}

TEST_CASE("switch commands take 0 or 1") {
    Settings s;
    CommandOutcome o = apply_command(s, "mvr_debug", "1");
    CHECK(o.kind == Kind::Applied);
    REQUIRE(o.change.has_value());
    CHECK(o.change->path == "DebugOptions");
    CHECK(o.change->text == "true");
    CHECK(s.debug.debug);

    o = apply_command(s, "mvr_debug", "1");
    CHECK(o.kind == Kind::Applied);
    CHECK_FALSE(o.change.has_value()); // no change

    CHECK(apply_command(s, "mvr_debug", "2").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_debug", nullptr).kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_unknown", "1").kind == Kind::Unknown);

    for (const char* name :
         {"mvr_mirrorviewsym", "mvr_clipblinding", "mvr_snaprotation", "mvr_inputrelativewalking",
          "mvr_inputstrafing", "mvr_togglegrips", "mvr_togglecrouch", "mvr_skipintro"}) {
        CHECK(apply_command(s, name, "0").kind == Kind::Applied);
    }
    CHECK_FALSE(s.rendering.symmetric_mirror_view);
    CHECK_FALSE(s.rendering.clipping_blinders);
    CHECK_FALSE(s.input.snap_rotation);
    CHECK_FALSE(s.input.vr_input_relative_walking);
    CHECK_FALSE(s.input.vr_input_strafing);
    CHECK(apply_command(s, "mvr_clipblinding", "1").effect ==
          omvr::core::config::CommandEffect::ApplyBlinders);
}

TEST_CASE("mvr_mirrorview takes the enum's integers") {
    Settings s;
    const CommandOutcome o = apply_command(s, "mvr_mirrorview", "2");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.change->text == "LeftEye");
    CHECK(s.rendering.mirror_view_type == MirrorViewType::LeftEye);
    CHECK(apply_command(s, "mvr_mirrorview", "3").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_mirrorview", "-1").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_mirrorview", "x").kind == Kind::Refused);
}

TEST_CASE("mvr_pixelscale takes a number in range") {
    Settings s;
    const CommandOutcome o = apply_command(s, "mvr_pixelscale", "1.25");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.change->path == "Rendering.PixelScale");
    CHECK(o.change->text == "1.25");
    CHECK(s.rendering.pixel_scale == 1.25f);
    CHECK(apply_command(s, "mvr_pixelscale", "0.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_pixelscale", "4").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_pixelscale", "4.5").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_pixelscale", "0.4").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_pixelscale", "abc").kind == Kind::Refused);
}

TEST_CASE("mvr_snaprotationsegments rounds down to a step") {
    Settings s;
    const CommandOutcome same = apply_command(s, "mvr_snaprotationsegments", "13");
    CHECK(same.kind == Kind::Applied);
    CHECK_FALSE(same.change.has_value()); // 13 rounds down to 12, the value already set
    CHECK(s.input.snap_rotation_segments == 12);
    CHECK(apply_command(s, "mvr_snaprotationsegments", "100").change->text == "24");
    CHECK(apply_command(s, "mvr_snaprotationsegments", "5").change->text == "4");
    CHECK(apply_command(s, "mvr_snaprotationsegments", "2").change->text == "2");
    CHECK(apply_command(s, "mvr_snaprotationsegments", "1").kind == Kind::Refused);
}

// Spec 22 §3.3, 21 §15.3: each v2 key's command takes the key's own range
// (22 §3.1, 21 §15.1, §15.2); a value outside it changes nothing.
TEST_CASE("v2 number commands take the key's range, and 0 where it means off") {
    Settings s;
    CommandOutcome o = apply_command(s, "mvr_smoothturnspeed", "45");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.change->path == "Comfort.SmoothTurnSpeed");
    CHECK(o.change->text == "45.00");
    CHECK(s.comfort.smooth_turn_speed == 45.0f);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "0").kind == Kind::Applied);
    CHECK(s.comfort.smooth_turn_speed == 0.0f);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "30").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "360").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "29.9").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "361").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "-30").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_smoothturnspeed", "fast").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_smoothturnspeed", nullptr).kind == Kind::Refused);
    CHECK(s.comfort.smooth_turn_speed == 360.0f);

    o = apply_command(s, "mvr_vignette", "0.3");
    CHECK(o.change->text == "0.30");
    CHECK(s.comfort.vignette == 0.3f);
    CHECK(apply_command(s, "mvr_vignette", "1").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_vignette", "1.1").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_vignette", "-0.1").kind == Kind::Refused);

    CHECK(apply_command(s, "mvr_standingeyeheight", "1.75").kind == Kind::Applied);
    CHECK(s.comfort.standing_eye_height == 1.75f);
    CHECK(apply_command(s, "mvr_standingeyeheight", "0").kind == Kind::Applied); // not calibrated
    CHECK(apply_command(s, "mvr_standingeyeheight", "0.9").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_standingeyeheight", "2.3").kind == Kind::Refused);

    CHECK(apply_command(s, "mvr_worldscale", "1.25").kind == Kind::Applied);
    CHECK(s.comfort.world_scale == 1.25f);
    CHECK(apply_command(s, "mvr_worldscale", "0.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_worldscale", "1.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_worldscale", "0.4").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_worldscale", "1.6").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_worldscale", "0").kind == Kind::Refused); // 0 is no scale

    CHECK(apply_command(s, "mvr_throwstrength", "2").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_throwstrength", "0.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_throwstrength", "2.1").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_throwstrength", "0.4").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_handsmoothing", "0.7").kind == Kind::Applied);
    CHECK(s.hands.smoothing == 0.7f);
    CHECK(apply_command(s, "mvr_handsmoothing", "1.5").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_hapticsstrength", "0").kind == Kind::Applied);
    CHECK(s.hands.haptics.strength == 0.0f);
    CHECK(apply_command(s, "mvr_hapticsstrength", "1.01").kind == Kind::Refused);

    CHECK(apply_command(s, "mvr_eyeforward", "-0.2").kind == Kind::Applied);
    CHECK(s.body.eye_forward == -0.2f);
    CHECK(apply_command(s, "mvr_eyeforward", "-0.21").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_eyeforward", "0.21").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_seatedeyeraise", "0.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_seatedeyeraise", "-0.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_seatedeyeraise", "0.55").kind == Kind::Refused);
}

TEST_CASE("v2 integer commands take whole numbers in range, and 0 where it means off") {
    Settings s;
    const CommandOutcome o = apply_command(s, "mvr_snapturnrepeat", "500");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.change->path == "Comfort.SnapTurnRepeatMs");
    CHECK(o.change->text == "500");
    CHECK(s.comfort.snap_turn_repeat_ms == 500);
    CHECK(apply_command(s, "mvr_snapturnrepeat", "0").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_snapturnrepeat", "150").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_snapturnrepeat", "1000").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_snapturnrepeat", "149").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_snapturnrepeat", "1001").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_snapturnrepeat", "200.5").kind == Kind::Refused);
    CHECK(s.comfort.snap_turn_repeat_ms == 1000);
}

// Spec 24 §10: the Graphics page's commands (user decision of 2026-10-18).
TEST_CASE("Graphics commands: switches, heap sizes in range or 0, the 2D effects' effect") {
    Settings s;
    for (const char* name : {"mvr_enginesettingsoverride", "mvr_shadowmap4096", "mvr_sharpreflections"}) {
        const CommandOutcome o = apply_command(s, name, "1");
        CHECK(o.kind == Kind::Applied);
        CHECK(o.effect == CommandEffect::None);
        CHECK(apply_command(s, name, "2").kind == Kind::Refused);
    }
    CHECK(s.graphics.engine_settings_override);
    CHECK(s.graphics.shadow_map_4096);
    CHECK(s.graphics.sharp_reflections);

    CommandOutcome o = apply_command(s, "mvr_streamingtextureheap", "300");
    CHECK(o.change->path == "Graphics.StreamingTextureHeapMB");
    CHECK(o.change->text == "300");
    CHECK(apply_command(s, "mvr_streamingtextureheap", "1024").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_streamingtextureheap", "299").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_streamingtextureheap", "1025").kind == Kind::Refused);
    CHECK(s.graphics.streaming_texture_heap_mb == 1024);
    CHECK(apply_command(s, "mvr_streamingtextureheap", "0").kind == Kind::Applied);
    CHECK(s.graphics.streaming_texture_heap_mb == 0);
    CHECK(apply_command(s, "mvr_streamingmodelheap", "128").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_streamingmodelheap", "512").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_streamingmodelheap", "127").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_streamingmodelheap", "513").kind == Kind::Refused);
    CHECK(s.graphics.streaming_model_heap_mb == 512);

    for (const char* name : {"mvr_lensflares", "mvr_chromaticaberration", "mvr_filmgrain"}) {
        const CommandOutcome off = apply_command(s, name, "0");
        CHECK(off.kind == Kind::Applied);
        CHECK(off.change->text == "false"); // these default on (24 §10)
        CHECK(off.effect == CommandEffect::ApplyPostFx);
        CHECK(apply_command(s, name, "2").kind == Kind::Refused);
    }
    CHECK_FALSE(s.graphics.lens_flares);
    CHECK_FALSE(s.graphics.chromatic_aberration);
    CHECK_FALSE(s.graphics.film_grain);
}

TEST_CASE("a 2D effect command reaches the effect hooks' keys at once") {
    MemoryFileStore files;
    RecordingLog log;
    omvr::core::config::SettingsStore store(files, log);
    store.initialize(L"C:\\game");
    FakeGameSignals signals;
    FakeRuntimeState state;
    DisplayReset reset(signals, state, log);
    FakePlaySpaceControl play_space;
    omvr::core::render::PostFxEffects post_fx;
    Commands commands(store, reset, signals, play_space, post_fx, log);
    char grain[] = "mvr_filmgrain 0";
    commands.run(grain);
    CHECK_FALSE(post_fx.keys().film_grain);
    CHECK(post_fx.keys().lens_flares);
    CHECK(post_fx.keys().chromatic_aberration);
    CHECK(files.writes == 1);
    CHECK(log.contains("[script_bridge] Graphics.FilmGrain changed to false by command"));
    // A command without the effect leaves the keys alone.
    char shadows[] = "mvr_shadowmap4096 1";
    commands.run(shadows);
    CHECK_FALSE(post_fx.keys().film_grain);
    CHECK(store.current().graphics.shadow_map_4096);
}

TEST_CASE("v2 list and switch commands") {
    Settings s;
    CommandOutcome o = apply_command(s, "mvr_playmode", "1");
    CHECK(o.change->text == "Standing");
    CHECK(s.comfort.play_mode == PlayMode::Standing);
    CHECK(apply_command(s, "mvr_playmode", "2").kind == Kind::Refused);
    o = apply_command(s, "mvr_flashlightaim", "2");
    CHECK(o.change->text == "RightHand");
    CHECK(s.hands.flashlight_aim == FlashlightAim::RightHand);
    CHECK(apply_command(s, "mvr_flashlightaim", "3").kind == Kind::Refused);
    o = apply_command(s, "mvr_bodyvisible", "3");
    CHECK(o.change->path == "Body.Visible");
    CHECK(o.change->text == "None");
    CHECK(s.body.visible == BodyVisible::None);
    CHECK(apply_command(s, "mvr_bodyvisible", "4").kind == Kind::Refused);

    for (const char* name :
         {"mvr_snapturnfade", "mvr_interactionawareturning", "mvr_dpademulation", "mvr_recentergesture",
          "mvr_hudtogglegesture", "mvr_holstergestures", "mvr_hidinglean", "mvr_holdbreathgesture",
          "mvr_controllerglyphs", "mvr_vroffhotkey", "mvr_crashsafelog", "mvr_hands", "mvr_wristhealth",
          "mvr_haptics", "mvr_roomscale", "mvr_realcrouch"}) {
        CHECK(apply_command(s, name, "1").kind == Kind::Applied);
        CHECK(apply_command(s, name, "2").kind == Kind::Refused);
    }
    CHECK(s.comfort.snap_turn_fade);
    CHECK(s.controls.crash_safe_log);
    CHECK(s.hands.enabled);
    CHECK(s.hands.haptics.enabled);
    CHECK(s.body.real_crouch);
    // These default on (21 §15.1).
    for (const char* name : {"mvr_aimfromhand", "mvr_trackerinhand", "mvr_meleebymotion", "mvr_throwbymotion",
                             "mvr_hapticsgamerumble", "mvr_hapticsdamage", "mvr_hapticstracker",
                             "mvr_hapticsfiring", "mvr_hapticshacking"}) {
        CHECK(apply_command(s, name, "0").change->text == "false");
    }
    CHECK_FALSE(s.hands.haptics.hacking);
}

TEST_CASE("mvr_calibrateheight asks for a calibration and saves nothing") {
    Settings s;
    const CommandOutcome o = apply_command(s, kCalibrateHeightCommand, nullptr);
    CHECK(o.kind == Kind::Applied);
    CHECK(o.effect == CommandEffect::CalibrateHeight);
    CHECK_FALSE(o.change.has_value());
    CHECK(apply_command(s, kCalibrateHeightCommand, "1").kind == Kind::Applied); // a value is ignored

    MemoryFileStore files;
    RecordingLog log;
    omvr::core::config::SettingsStore store(files, log);
    store.initialize(L"C:\\game");
    FakeGameSignals signals;
    FakeRuntimeState state;
    DisplayReset reset(signals, state, log);
    FakePlaySpaceControl play_space;
    omvr::core::render::PostFxEffects post_fx;
    Commands commands(store, reset, signals, play_space, post_fx, log);
    char cmd[] = "mvr_calibrateheight";
    commands.run(cmd);
    CHECK(play_space.requests == 1);
    CHECK(files.writes == 0);
    CHECK(signals.exits == 0);
    CHECK(log.lines.back() == "[script_bridge] standing height calibration requested by command");

    char vignette[] = "mvr_vignette 0.5";
    commands.run(vignette);
    CHECK(store.current().comfort.vignette == 0.5f);
    CHECK(files.writes == 1);
    CHECK(log.contains("[script_bridge] Comfort.Vignette changed to 0.50 by command"));
}

TEST_CASE("the push sends each v2 value with its type, the -vrscale switch and the calibration") {
    RecordingScriptVariables vars;
    FakeGameSignals signals;
    FakeRuntimeState state;
    RecordingLog log;
    DisplayReset reset(signals, state, log);
    omvr::core::input::Glyphs glyphs;
    omvr::core::input::ControlSchemes schemes(glyphs);
    FakePlaySpaceControl play_space;
    play_space.scale_switch = 1.2f;
    play_space.calibration  = {ports::CalibrationPageState::Counting, 2};
    Settings s;
    s.comfort.smooth_turn_speed          = 45.0f;
    s.comfort.play_mode                  = PlayMode::Standing;
    s.comfort.standing_eye_height        = 1.72f;
    s.controls.dpad_emulation            = true;
    s.hands.flashlight_aim               = FlashlightAim::LeftHand;
    s.hands.haptics.strength             = 0.5f;
    s.body.visible                       = BodyVisible::HandsOnly;
    s.graphics.streaming_texture_heap_mb = 512;
    s.graphics.lens_flares               = false;
    SettingsPush push({vars, reset, schemes, s, state, play_space, log});
    push.push(ports::UiHandle{1});

    const auto has = [&](const std::string& call) {
        return std::ranges::find(vars.calls, call) != vars.calls.end();
    };
    CHECK(has("setMotherSmoothTurnSpeed=45.000000"));
    CHECK(has("setMotherSnapTurnRepeatMs=300"));
    CHECK(has("setMotherSnapTurnFade=false"));
    CHECK(has("setMotherVignette=0.000000"));
    CHECK(has("setMotherPlayMode=1"));
    CHECK(has("setMotherStandingEyeHeight=1.720000"));
    CHECK(has("setMotherWorldScale=1.000000"));
    CHECK(has("setMotherDpadEmulation=true"));
    CHECK(has("setMotherCrashSafeLog=false"));
    CHECK(has("setMotherHandsEnabled=false"));
    CHECK(has("setMotherAimFromHand=true"));
    CHECK(has("setMotherFlashlightAim=1"));
    CHECK(has("setMotherThrowStrength=1.000000"));
    CHECK(has("setMotherHandSmoothing=0.000000"));
    CHECK(has("setMotherHapticsEnabled=false"));
    CHECK(has("setMotherHapticsStrength=0.500000"));
    CHECK(has("setMotherHapticsHacking=true"));
    CHECK(has("setMotherBodyVisible=2"));
    CHECK(has("setMotherEyeForward=0.000000"));
    CHECK(has("setMotherRealCrouch=false"));
    CHECK(has("setMotherEngineSettingsOverride=false"));
    CHECK(has("setMotherShadowMap4096=false"));
    CHECK(has("setMotherSharpReflections=false"));
    CHECK(has("setMotherStreamingTextureHeapMB=512"));
    CHECK(has("setMotherStreamingModelHeapMB=0"));
    CHECK(has("setMotherLensFlares=false"));
    CHECK(has("setMotherChromaticAberration=true"));
    CHECK(has("setMotherFilmGrain=true"));
    CHECK(has("setMotherWorldScaleSwitch=1.200000"));
    CHECK(has("setMotherCalibrationState=1"));
    CHECK(has("setMotherCalibrationSeconds=2"));

    // Without the switch the page is told 0.
    RecordingScriptVariables plain;
    FakePlaySpaceControl no_switch;
    SettingsPush without({plain, reset, schemes, s, state, no_switch, log});
    without.push(ports::UiHandle{1});
    CHECK(std::ranges::find(plain.calls, "setMotherWorldScaleSwitch=0.000000") != plain.calls.end());
}

TEST_CASE("mvr_vrruntime maps the options page's entries and quits") {
    Settings s;
    CommandOutcome o = apply_command(s, "mvr_vrruntime", "1");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.effect == omvr::core::config::CommandEffect::QuitGame);
    CHECK(s.vr_runtime == VRRuntime::OpenXR);
    CHECK(o.change->text == "OpenXR");
    CHECK(apply_command(s, "mvr_vrruntime", "2").change->text == "OpenVR");
    CHECK(apply_command(s, "mvr_vrruntime", "0").change->text == "None");
    CHECK(apply_command(s, "mvr_vrruntime", "3").kind == Kind::Refused);
}

TEST_CASE("the runtime menu entries") {
    CHECK(ui_runtime_value(VRRuntime::OpenXR) == RuntimeEntry::OpenXr);
    CHECK(ui_runtime_value(VRRuntime::Oculus) == RuntimeEntry::OpenXr);
    CHECK(ui_runtime_value(VRRuntime::OpenVR) == RuntimeEntry::OpenVr);
    CHECK(ui_runtime_value(VRRuntime::None) == RuntimeEntry::Off);
    CHECK(runtime_from_ui(0) == VRRuntime::None);
    CHECK(runtime_from_ui(1) == VRRuntime::OpenXR);
    CHECK(runtime_from_ui(2) == VRRuntime::OpenVR);
    CHECK_FALSE(runtime_from_ui(5).has_value());

    Settings s;
    FakeRuntimeState state;
    CHECK(running_runtime(s, state) == VRRuntime::OpenVR);
    state.chosen_runtime = Chosen::OpenXR;
    CHECK(running_runtime(s, state) == VRRuntime::OpenXR);
    state.chosen_runtime = Chosen::Oculus;
    CHECK(running_runtime(s, state) == VRRuntime::None);
    state.chosen_runtime = Chosen::OpenVR;
    state.has_failed     = true;
    CHECK(running_runtime(s, state) == VRRuntime::None);
    state.has_failed = false;
    s.vr_runtime     = VRRuntime::None;
    CHECK(running_runtime(s, state) == VRRuntime::None);
    state.forced_runtime = VRRuntime::OpenVR; // a switch overrides the file
    CHECK(running_runtime(s, state) == VRRuntime::OpenVR);
}

TEST_CASE("display reset: only on a change, only with VR") {
    FakeGameSignals signals;
    FakeRuntimeState state;
    RecordingLog log;
    DisplayReset reset(signals, state, log);
    Settings s;
    reset.reset_if_changed(s); // seeds
    reset.reset_if_changed(s);
    CHECK(signals.resets == 0);

    s.rendering.pixel_scale = 1.5f;
    reset.reset_if_changed(s);
    CHECK(signals.resets == 1);
    CHECK(log.lines.back() == "[script_bridge] display reset raised; changed: Rendering.PixelScale");

    s.rendering.mirror_view_type      = MirrorViewType::BothEyes;
    s.rendering.symmetric_mirror_view = false;
    reset.reset_if_changed(s);
    CHECK(log.lines.back() ==
          "[script_bridge] display reset raised; changed: Rendering.MirrorViewType "
          "Rendering.SymmetricMirrorView");

    s.vr_runtime            = VRRuntime::None;
    s.rendering.pixel_scale = 2.0f;
    reset.reset_if_changed(s);
    CHECK(signals.resets == 2); // no VR this run: nothing to rebuild

    signals.can_reset       = false;
    s.vr_runtime            = VRRuntime::OpenVR;
    s.rendering.pixel_scale = 1.0f;
    reset.reset_if_changed(s);
    CHECK(signals.resets == 2);
}

TEST_CASE("the command runner saves, then quits, after mvr_vrruntime") {
    std::vector<std::string> events;
    MemoryFileStore files;
    files.events = &events;
    RecordingLog log;
    omvr::core::config::SettingsStore store(files, log);
    store.initialize(L"C:\\game");
    FakeGameSignals signals;
    signals.events = &events;
    FakeRuntimeState state;
    DisplayReset reset(signals, state, log);
    FakePlaySpaceControl play_space;
    omvr::core::render::PostFxEffects post_fx;
    Commands commands(store, reset, signals, play_space, post_fx, log);

    char cmd[] = "mvr_vrruntime 1 trailing";
    commands.run(cmd);
    CHECK(store.current().vr_runtime == VRRuntime::OpenXR);
    REQUIRE(events.size() == 2);
    CHECK(events[0] == "write");
    CHECK(events[1] == "exit");
    CHECK(log.contains("[script_bridge] VRRuntime changed to OpenXR by command"));

    char refused[] = "mvr_debug 7";
    commands.run(refused);
    CHECK(log.lines.back() == "[script_bridge] mvr_debug 7: value not accepted, nothing changed");
    char unknown[] = "mvr_what 1";
    commands.run(unknown);
    CHECK(log.lines.back() == "[script_bridge] unknown command mvr_what ignored");
    char none[] = "mvr_debug";
    commands.run(none);
    CHECK(log.lines.back() == "[script_bridge] mvr_debug (none): value not accepted, nothing changed");
    CHECK(files.writes == 1);
}

TEST_CASE("the settings push sets every variable, then the controls help") {
    RecordingScriptVariables vars;
    FakeGameSignals signals;
    FakeRuntimeState state;
    RecordingLog log;
    DisplayReset reset(signals, state, log);
    omvr::core::input::Glyphs glyphs;
    omvr::core::input::ControlSchemes schemes(glyphs);
    Settings s;
    s.rendering.pixel_scale = 1.5f;
    FakePlaySpaceControl play_space;
    SettingsPush push({vars, reset, schemes, s, state, play_space, log});

    push.push(ports::UiHandle{});
    CHECK(vars.calls.empty());
    push.push(ports::UiHandle{});
    CHECK(log.lines.size() == 1); // logged once

    push.push(ports::UiHandle{1});
    const std::vector<std::string> head(vars.calls.begin(), vars.calls.begin() + 14);
    CHECK(head == std::vector<std::string>{
                      "setMotherVROptionsEnabled=true",
                      "setMotherSetVRRuntime=2",
                      "setMotherDebug=0",
                      "setMotherSkipIntro=false",
                      "setMotherMirrorView=1",
                      "setMotherMirrorViewSym=true",
                      "setMotherPixelScale=1.500000",
                      "setMotherClipBlinding=true",
                      "setMotherSnapRotation=true",
                      "setMotherSnapRotationSegments=12",
                      "setMotherInputRelativeWalking=true",
                      "setMotherInputStrafing=true",
                      "setMotherToggleGrips=false",
                      "setMotherToggleCrouch=false",
                  });
    // Then the v2 pages: 38 settings, 3 page states, the 8 settings of the
    // Graphics page and the 12 of the Interaction and HUD pages (pushed
    // before the help rows, which the page shows on its own sub-page).
    constexpr std::size_t kPagePushes = 38 + 3 + 8 + 12;
    CHECK(vars.calls[14] == "setMotherSmoothTurnSpeed=0.000000");
    CHECK(vars.calls[14 + kPagePushes] == "addMotherViewControl=Move|L Touchpad");
    CHECK(vars.calls.size() == 14 + kPagePushes + schemes.control_help().size());

    // A row the game refuses stops the list.
    RecordingScriptVariables short_vars;
    short_vars.row_limit = 2;
    SettingsPush stops({short_vars, reset, schemes, s, state, play_space, log});
    stops.push(ports::UiHandle{1});
    CHECK(short_vars.rows == 2);
}

TEST_CASE("PAK2 round trip") {
    const std::vector<OutEntry> entries = {{"a", bytes_of("abc")}, {"bb", bytes_of("d")}};
    const std::vector<std::byte> pak    = build_pak(16, entries);
    // Header 16, names "a\0bb\0" 5, table 8: entry a at 32, bb at 48.
    CHECK(pak.size() == 49u);
    CHECK(string_of(std::span(pak).first(4)) == "PAK2");

    const auto read = read_pak(pak);
    REQUIRE(read.has_value());
    CHECK(read->size() == 2u);
    CHECK(string_of(read->at("a")) == "abc");
    CHECK(string_of(read->at("bb")) == "d");

    std::vector<std::byte> bad = pak;
    bad[0]                     = std::byte{'X'};
    CHECK_FALSE(read_pak(bad).has_value());
    CHECK_FALSE(read_pak(std::span(pak).first(20)).has_value());
    std::vector<std::byte> zero_align = pak;
    std::memset(zero_align.data() + 12, 0, 4);
    CHECK_FALSE(read_pak(zero_align).has_value());
}

// A UI patch (MVRUIP01) built byte by byte, as scripts/ui_patch.py writes it.
class PatchWriter {
public:
    PatchWriter(std::uint32_t align, std::uint32_t count) {
        text("MVRUIP01");
        u32(align);
        u32(count);
    }
    void entry(std::string_view name, PatchEntry kind) {
        u16(static_cast<std::uint16_t>(name.size()));
        text(name);
        u8(static_cast<std::uint8_t>(kind));
    }
    void u8(std::uint8_t v) { out.push_back(std::byte{v}); }
    void u16(std::uint16_t v) { raw(&v, sizeof(v)); }
    void u32(std::uint32_t v) { raw(&v, sizeof(v)); }
    void text(std::string_view s) { raw(s.data(), s.size()); }

    std::vector<std::byte> out;

private:
    void raw(const void* p, std::size_t n) {
        const auto* b = static_cast<const std::byte*>(p);
        out.insert(out.end(), b, b + n);
    }
};

std::uint32_t crc_of(std::string_view s) { return omvr::util::crc32(bytes_of(s)); }

TEST_CASE("the UI patch copies, rewrites and adds entries") {
    FakeArchiveFiles files;
    RecordingLog log;
    const std::filesystem::path stock  = L"DATA\\UI.PAK";
    const std::filesystem::path target = L"DATA\\MOTHER.PAK";
    files.files[stock] = build_pak(4, {{"keep", bytes_of("hello")}, {"edit", bytes_of("abcdef")}});

    PatchWriter p(4, 3);
    p.entry("keep", PatchEntry::Stock);
    p.u32(5);
    p.u32(crc_of("hello"));
    p.entry("edit", PatchEntry::Delta);
    p.u32(6);
    p.u32(crc_of("abcdef"));
    p.u32(5); // output size
    p.u32(2); // operations
    p.u8(static_cast<std::uint8_t>(DeltaOp::Copy));
    p.u32(0);
    p.u32(3);
    p.u8(static_cast<std::uint8_t>(DeltaOp::Insert));
    p.u32(2);
    p.text("XY");
    p.entry("new", PatchEntry::New);
    p.u32(3);
    p.text("new");

    REQUIRE(build_ui_archive(files, log, stock, p.out, target));
    const auto built = read_pak(files.files.at(target));
    REQUIRE(built.has_value());
    CHECK(string_of(built->at("keep")) == "hello");
    CHECK(string_of(built->at("edit")) == "abcXY");
    CHECK(string_of(built->at("new")) == "new");
    CHECK(log.contains("ui patch: built"));
}

TEST_CASE("the UI patch refuses another stock version and a malformed patch") {
    FakeArchiveFiles files;
    RecordingLog log;
    const std::filesystem::path stock  = L"UI.PAK";
    const std::filesystem::path target = L"MOTHER.PAK";
    files.files[stock]                 = build_pak(4, {{"keep", bytes_of("HELLO")}});

    PatchWriter p(4, 1);
    p.entry("keep", PatchEntry::Stock);
    p.u32(5);
    p.u32(crc_of("hello"));
    CHECK_FALSE(build_ui_archive(files, log, stock, p.out, target));
    CHECK(log.contains("ui patch: keep in UI.PAK is not the version the patch was made for"));
    CHECK_FALSE(files.files.contains(target));

    std::vector<std::byte> bad = p.out;
    bad[0]                     = std::byte{'X'};
    CHECK_FALSE(build_ui_archive(files, log, stock, bad, target));
    CHECK(log.lines.back() == "ui patch: embedded patch is malformed");

    CHECK_FALSE(build_ui_archive(files, log, L"missing.pak", p.out, target));
    CHECK(log.lines.back() == "ui patch: cannot read missing.pak");

    // A copy that reaches past the stock entry.
    files.files[stock] = build_pak(4, {{"e", bytes_of("abc")}});
    PatchWriter d(4, 1);
    d.entry("e", PatchEntry::Delta);
    d.u32(3);
    d.u32(crc_of("abc"));
    d.u32(4);
    d.u32(1);
    d.u8(static_cast<std::uint8_t>(DeltaOp::Copy));
    d.u32(1);
    d.u32(4);
    CHECK_FALSE(build_ui_archive(files, log, stock, d.out, target));
}

TEST_CASE("the UI patch reports a temporary file it cannot create") {
    FakeArchiveFiles files;
    RecordingLog log;
    files.files[L"UI.PAK"] = build_pak(4, {{"n", bytes_of("x")}});
    files.fail_create      = true;
    PatchWriter p(4, 1);
    p.entry("n", PatchEntry::New);
    p.u32(1);
    p.text("y");
    CHECK_FALSE(build_ui_archive(files, log, L"UI.PAK", p.out, L"MOTHER.PAK"));
    CHECK(log.contains("ui patch: cannot create MOTHER.PAK.tmp err=0x00000005"));
}

// Spec 23 §4.2 to §4.4 and §8: with "Reduced" the steer and re-align are not
// applied, except during a cutscene or level fade; "Game" keeps every
// takeover; an interaction in progress keeps the rule it started with.
TEST_CASE("camera takeovers: Game keeps all, Reduced skips outside cutscenes") {
    using omvr::core::config::CameraTakeovers;
    CameraTakeoverRule rule;
    CHECK_FALSE(rule.suppress({.setting = CameraTakeovers::Game}));
    CHECK_FALSE(rule.suppress({.setting = CameraTakeovers::Game, .in_interaction = true}));
    CHECK(rule.suppress({.setting = CameraTakeovers::Reduced}));
    CHECK(rule.suppress({.setting = CameraTakeovers::Reduced, .in_interaction = true}));
    CHECK_FALSE(rule.suppress({.setting = CameraTakeovers::Reduced, .cutscene_or_fade = true}));
    CHECK_FALSE(rule.suppress(
        {.setting = CameraTakeovers::Reduced, .in_interaction = true, .cutscene_or_fade = true}));
}

TEST_CASE("camera takeovers: an interaction keeps the rule it started with") {
    using omvr::core::config::CameraTakeovers;
    CameraTakeoverRule rule;
    // Started under "Game", "Reduced" chosen in the pause menu: the
    // interaction's end re-align stays the game's.
    rule.observe({.setting = CameraTakeovers::Game, .in_interaction = true});
    CHECK_FALSE(rule.suppress({.setting = CameraTakeovers::Reduced, .in_interaction = true}));
    CHECK(rule.rule({.setting = CameraTakeovers::Reduced, .in_interaction = true}) == CameraTakeovers::Game);
    // It ends: the new setting applies from then on.
    rule.observe({.setting = CameraTakeovers::Reduced});
    CHECK(rule.suppress({.setting = CameraTakeovers::Reduced}));
    // Started under "Reduced", back to "Game" during it: still reduced.
    rule.observe({.setting = CameraTakeovers::Reduced, .in_interaction = true});
    CHECK(rule.suppress({.setting = CameraTakeovers::Game, .in_interaction = true}));
    rule.observe({.setting = CameraTakeovers::Game});
    CHECK_FALSE(rule.suppress({.setting = CameraTakeovers::Game}));
}

TEST_CASE("camera takeovers: the first decision of an interaction takes its rule") {
    using omvr::core::config::CameraTakeovers;
    CameraTakeoverRule rule;
    CHECK(rule.suppress({.setting = CameraTakeovers::Reduced, .in_interaction = true}));
    CHECK(rule.suppress({.setting = CameraTakeovers::Game, .in_interaction = true}));
    CHECK(takeover_name(Takeover::StealCameraSteer) == "StealCamera steer");
    CHECK(takeover_name(Takeover::ResetRealign) == "camera reset re-align");
}

// Records which way each box was shown.
struct RecordingDialogs final : ports::Dialogs {
    std::vector<std::string> waited;
    std::vector<std::string> async;
    void post(std::string text, ports::DialogKind, std::move_only_function<void()>,
              std::string_view) noexcept override {
        async.push_back(std::move(text));
    }
    void show_async(std::string text, ports::DialogKind, std::string_view) noexcept override {
        async.push_back(std::move(text));
    }
    int show_now(std::string_view text, ports::DialogKind, std::string_view) noexcept override {
        waited.emplace_back(text);
        return 1;
    }
    void wait_posted() noexcept override {}
};

TEST_CASE("runtime failure: the init report waits, the late one does not") {
    using omvr::core::vr::DetectResult;
    const omvr::core::vr::InitFailure failure{"no session", -2};
    RecordingDialogs init;
    report_init_failure(init, DetectResult::Chosen::OpenXR, failure, ReportWait::UntilClosed);
    REQUIRE(init.waited.size() == 1);
    CHECK(init.async.empty());

    RecordingDialogs late;
    report_init_failure(late, DetectResult::Chosen::OpenXR, failure, ReportWait::No);
    CHECK(late.waited.empty());
    REQUIRE(late.async.size() == 1);
    CHECK(late.async[0] == init.waited[0]); // the same message either way
}

} // namespace
