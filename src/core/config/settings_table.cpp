#include "core/config/settings_table.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace omvr::core::config {

namespace {

// Where each setting lives in Settings.
VRRuntime& vr_runtime(Settings& s) noexcept { return s.vr_runtime; }
MirrorViewType& mirror_view_type(Settings& s) noexcept { return s.rendering.mirror_view_type; }
bool& symmetric_mirror_view(Settings& s) noexcept { return s.rendering.symmetric_mirror_view; }
float& pixel_scale(Settings& s) noexcept { return s.rendering.pixel_scale; }
bool& clipping_blinders(Settings& s) noexcept { return s.rendering.clipping_blinders; }
StereoAA& stereo_aa(Settings& s) noexcept { return s.rendering.stereo_aa; }
bool& snap_rotation(Settings& s) noexcept { return s.input.snap_rotation; }
int& snap_rotation_segments(Settings& s) noexcept { return s.input.snap_rotation_segments; }
bool& relative_walking(Settings& s) noexcept { return s.input.vr_input_relative_walking; }
bool& strafing(Settings& s) noexcept { return s.input.vr_input_strafing; }
bool& toggle_grips(Settings& s) noexcept { return s.input.toggle_grips; }
bool& toggle_crouch(Settings& s) noexcept { return s.input.toggle_crouch; }
bool& debug_options(Settings& s) noexcept { return s.debug.debug; }
bool& skip_startup_intro(Settings& s) noexcept { return s.debug.skip_startup_intro; }
bool& discord_rich_presence(Settings& s) noexcept { return s.discord_rich_presence; }
float& smooth_turn_speed(Settings& s) noexcept { return s.comfort.smooth_turn_speed; }
int& snap_turn_repeat_ms(Settings& s) noexcept { return s.comfort.snap_turn_repeat_ms; }
bool& snap_turn_fade(Settings& s) noexcept { return s.comfort.snap_turn_fade; }
float& vignette(Settings& s) noexcept { return s.comfort.vignette; }
PlayMode& play_mode(Settings& s) noexcept { return s.comfort.play_mode; }
float& standing_eye_height(Settings& s) noexcept { return s.comfort.standing_eye_height; }
float& world_scale(Settings& s) noexcept { return s.comfort.world_scale; }
bool& interaction_aware_turning(Settings& s) noexcept { return s.controls.interaction_aware_turning; }
bool& dpad_emulation(Settings& s) noexcept { return s.controls.dpad_emulation; }
bool& recenter_gesture(Settings& s) noexcept { return s.controls.recenter_gesture; }
bool& hud_toggle_gesture(Settings& s) noexcept { return s.controls.hud_toggle_gesture; }
bool& holster_gestures(Settings& s) noexcept { return s.controls.holster_gestures; }
bool& hiding_lean(Settings& s) noexcept { return s.controls.hiding_lean; }
bool& hold_breath_gesture(Settings& s) noexcept { return s.controls.hold_breath_gesture; }
bool& controller_glyphs(Settings& s) noexcept { return s.controls.controller_glyphs; }
bool& vr_off_hotkey(Settings& s) noexcept { return s.controls.vr_off_hotkey; }
bool& crash_safe_log(Settings& s) noexcept { return s.controls.crash_safe_log; }
bool& hands_enabled(Settings& s) noexcept { return s.hands.enabled; }
bool& aim_from_hand(Settings& s) noexcept { return s.hands.aim_from_hand; }
bool& tracker_in_hand(Settings& s) noexcept { return s.hands.tracker_in_hand; }
FlashlightAim& flashlight_aim(Settings& s) noexcept { return s.hands.flashlight_aim; }
bool& melee_by_motion(Settings& s) noexcept { return s.hands.melee_by_motion; }
bool& throw_by_motion(Settings& s) noexcept { return s.hands.throw_by_motion; }
float& throw_strength(Settings& s) noexcept { return s.hands.throw_strength; }
float& hand_smoothing(Settings& s) noexcept { return s.hands.smoothing; }
bool& wrist_health(Settings& s) noexcept { return s.hands.wrist_health; }
bool& haptics_enabled(Settings& s) noexcept { return s.hands.haptics.enabled; }
float& haptics_strength(Settings& s) noexcept { return s.hands.haptics.strength; }
bool& haptics_game_rumble(Settings& s) noexcept { return s.hands.haptics.game_rumble; }
bool& haptics_damage(Settings& s) noexcept { return s.hands.haptics.damage; }
bool& haptics_tracker(Settings& s) noexcept { return s.hands.haptics.tracker; }
bool& haptics_firing(Settings& s) noexcept { return s.hands.haptics.firing; }
bool& haptics_hacking(Settings& s) noexcept { return s.hands.haptics.hacking; }
BodyVisible& body_visible(Settings& s) noexcept { return s.body.visible; }
float& eye_forward(Settings& s) noexcept { return s.body.eye_forward; }
bool& room_scale(Settings& s) noexcept { return s.body.room_scale; }
float& seated_eye_raise(Settings& s) noexcept { return s.body.seated_eye_raise; }
bool& real_crouch(Settings& s) noexcept { return s.body.real_crouch; }
bool& engine_settings_override(Settings& s) noexcept { return s.graphics.engine_settings_override; }
bool& shadow_map_4096(Settings& s) noexcept { return s.graphics.shadow_map_4096; }
bool& sharp_reflections(Settings& s) noexcept { return s.graphics.sharp_reflections; }
int& streaming_texture_heap_mb(Settings& s) noexcept { return s.graphics.streaming_texture_heap_mb; }
int& streaming_model_heap_mb(Settings& s) noexcept { return s.graphics.streaming_model_heap_mb; }

// What the v2 commands accept (spec 22 §3.1, 21 §15): the keys' ranges,
// and 0 where it means off or not calibrated.
constexpr CommandRange kSmoothTurnSpeed{kSmoothTurnSpeedMin, kSmoothTurnSpeedMax, true};
constexpr CommandRange kSnapTurnRepeat{static_cast<float>(kSnapTurnRepeatMsMin),
                                       static_cast<float>(kSnapTurnRepeatMsMax), true};
constexpr CommandRange kVignette{0.0f, kVignetteMax};
constexpr CommandRange kStandingEyeHeight{kStandingEyeHeightMin, kStandingEyeHeightMax, true};
constexpr CommandRange kWorldScale{kWorldScaleMin, kWorldScaleMax};
constexpr CommandRange kThrowStrength{kThrowStrengthMin, kThrowStrengthMax};
constexpr CommandRange kHandSmoothing{0.0f, kHandSmoothingMax};
constexpr CommandRange kHapticStrength{0.0f, kHapticStrengthMax};
constexpr CommandRange kEyeForward{-kEyeForwardMax, kEyeForwardMax};
constexpr CommandRange kSeatedEyeRaise{-kSeatedEyeRaiseMax, kSeatedEyeRaiseMax};

// A row of the v2 pages: its command (named by this project, spec 22 §3.3)
// and the page's function its value is pushed to.
constexpr SettingRow page_row(Section section, std::string_view key, std::string_view path, Field field,
                              std::string_view command, std::string_view push,
                              CommandRule rule = CommandRule::Switch, CommandRange range = {}) {
    return SettingRow{section, key, path, field, command, rule, CommandEffect::None, range, push};
}

// Key names are the original's (spec 06 §3); "StereoAntiAliasing" and the
// OpenXR runtime value are this project's.
constexpr std::array kRows{
    SettingRow{Section::Settings, "VRRuntime", "VRRuntime", &vr_runtime, "mvr_vrruntime",
               CommandRule::RuntimeEntry, CommandEffect::QuitGame},
    SettingRow{Section::Rendering, "MirrorViewType", "Rendering.MirrorViewType", &mirror_view_type,
               "mvr_mirrorview", CommandRule::Choice},
    SettingRow{Section::Rendering, "SymmetricMirrorView", "Rendering.SymmetricMirrorView",
               &symmetric_mirror_view, "mvr_mirrorviewsym", CommandRule::Switch},
    SettingRow{Section::Rendering, "PixelScale", "Rendering.PixelScale", &pixel_scale, "mvr_pixelscale",
               CommandRule::PixelScale},
    SettingRow{Section::Rendering, "ClippingBlinders", "Rendering.ClippingBlinders", &clipping_blinders,
               "mvr_clipblinding", CommandRule::Switch, CommandEffect::ApplyBlinders},
    SettingRow{Section::Rendering, "StereoAntiAliasing", "Rendering.StereoAntiAliasing", &stereo_aa},
    SettingRow{Section::Input, "SnapRotation", "Input.SnapRotation", &snap_rotation, "mvr_snaprotation",
               CommandRule::Switch},
    SettingRow{Section::Input, "SnapRotationSegments", "Input.SnapRotationSegments", &snap_rotation_segments,
               "mvr_snaprotationsegments", CommandRule::Segments},
    SettingRow{Section::Input, "VRInputRelativeWalking", "Input.VRInputRelativeWalking", &relative_walking,
               "mvr_inputrelativewalking", CommandRule::Switch},
    SettingRow{Section::Input, "VRInputStrafing", "Input.VRInputStrafing", &strafing, "mvr_inputstrafing",
               CommandRule::Switch},
    SettingRow{Section::Input, "ToggleGrips", "Input.ToggleGrips", &toggle_grips, "mvr_togglegrips",
               CommandRule::Switch},
    SettingRow{Section::Input, "ToggleCrouch", "Input.ToggleCrouch", &toggle_crouch, "mvr_togglecrouch",
               CommandRule::Switch},
    // "Dev. Debug Options" on the options page.
    SettingRow{Section::Settings, "DebugOptions", "DebugOptions", &debug_options, "mvr_debug",
               CommandRule::Switch},
    SettingRow{Section::Settings, "SkipStartupIntro", "SkipStartupIntro", &skip_startup_intro,
               "mvr_skipintro", CommandRule::Switch},
    // Ours, file only (no options-page entry).
    SettingRow{Section::Settings, "DiscordRichPresence", "DiscordRichPresence", &discord_rich_presence},
    // v2 (spec 22 §3.1, 21 §15): the pages Comfort, Controls, Hands and Body
    // (22 §12, 21 §15.3) send these commands and receive these pushes.
    page_row(Section::Comfort, "SmoothTurnSpeed", "Comfort.SmoothTurnSpeed", &smooth_turn_speed,
             "mvr_smoothturnspeed", "setMotherSmoothTurnSpeed", CommandRule::Number, kSmoothTurnSpeed),
    page_row(Section::Comfort, "SnapTurnRepeatMs", "Comfort.SnapTurnRepeatMs", &snap_turn_repeat_ms,
             "mvr_snapturnrepeat", "setMotherSnapTurnRepeatMs", CommandRule::Integer, kSnapTurnRepeat),
    page_row(Section::Comfort, "SnapTurnFade", "Comfort.SnapTurnFade", &snap_turn_fade, "mvr_snapturnfade",
             "setMotherSnapTurnFade"),
    page_row(Section::Comfort, "Vignette", "Comfort.Vignette", &vignette, "mvr_vignette", "setMotherVignette",
             CommandRule::Number, kVignette),
    page_row(Section::Comfort, "PlayMode", "Comfort.PlayMode", &play_mode, "mvr_playmode",
             "setMotherPlayMode", CommandRule::Choice),
    page_row(Section::Comfort, "StandingEyeHeight", "Comfort.StandingEyeHeight", &standing_eye_height,
             "mvr_standingeyeheight", "setMotherStandingEyeHeight", CommandRule::Number, kStandingEyeHeight),
    page_row(Section::Comfort, "WorldScale", "Comfort.WorldScale", &world_scale, "mvr_worldscale",
             "setMotherWorldScale", CommandRule::Number, kWorldScale),
    page_row(Section::Controls, "InteractionAwareTurning", "Controls.InteractionAwareTurning",
             &interaction_aware_turning, "mvr_interactionawareturning", "setMotherInteractionAwareTurning"),
    page_row(Section::Controls, "DpadEmulation", "Controls.DpadEmulation", &dpad_emulation,
             "mvr_dpademulation", "setMotherDpadEmulation"),
    page_row(Section::Controls, "RecenterGesture", "Controls.RecenterGesture", &recenter_gesture,
             "mvr_recentergesture", "setMotherRecenterGesture"),
    page_row(Section::Controls, "HudToggleGesture", "Controls.HudToggleGesture", &hud_toggle_gesture,
             "mvr_hudtogglegesture", "setMotherHudToggleGesture"),
    page_row(Section::Controls, "HolsterGestures", "Controls.HolsterGestures", &holster_gestures,
             "mvr_holstergestures", "setMotherHolsterGestures"),
    page_row(Section::Controls, "HidingLean", "Controls.HidingLean", &hiding_lean, "mvr_hidinglean",
             "setMotherHidingLean"),
    page_row(Section::Controls, "HoldBreathGesture", "Controls.HoldBreathGesture", &hold_breath_gesture,
             "mvr_holdbreathgesture", "setMotherHoldBreathGesture"),
    page_row(Section::Controls, "ControllerGlyphs", "Controls.ControllerGlyphs", &controller_glyphs,
             "mvr_controllerglyphs", "setMotherControllerGlyphs"),
    page_row(Section::Controls, "VrOffHotkey", "Controls.VrOffHotkey", &vr_off_hotkey, "mvr_vroffhotkey",
             "setMotherVrOffHotkey"),
    page_row(Section::Controls, "CrashSafeLog", "Controls.CrashSafeLog", &crash_safe_log, "mvr_crashsafelog",
             "setMotherCrashSafeLog"),
    page_row(Section::Hands, "Enabled", "Hands.Enabled", &hands_enabled, "mvr_hands",
             "setMotherHandsEnabled"),
    page_row(Section::Hands, "AimFromHand", "Hands.AimFromHand", &aim_from_hand, "mvr_aimfromhand",
             "setMotherAimFromHand"),
    page_row(Section::Hands, "TrackerInHand", "Hands.TrackerInHand", &tracker_in_hand, "mvr_trackerinhand",
             "setMotherTrackerInHand"),
    page_row(Section::Hands, "FlashlightAim", "Hands.FlashlightAim", &flashlight_aim, "mvr_flashlightaim",
             "setMotherFlashlightAim", CommandRule::Choice),
    page_row(Section::Hands, "MeleeByMotion", "Hands.MeleeByMotion", &melee_by_motion, "mvr_meleebymotion",
             "setMotherMeleeByMotion"),
    page_row(Section::Hands, "ThrowByMotion", "Hands.ThrowByMotion", &throw_by_motion, "mvr_throwbymotion",
             "setMotherThrowByMotion"),
    page_row(Section::Hands, "ThrowStrength", "Hands.ThrowStrength", &throw_strength, "mvr_throwstrength",
             "setMotherThrowStrength", CommandRule::Number, kThrowStrength),
    page_row(Section::Hands, "Smoothing", "Hands.Smoothing", &hand_smoothing, "mvr_handsmoothing",
             "setMotherHandSmoothing", CommandRule::Number, kHandSmoothing),
    page_row(Section::Hands, "WristHealth", "Hands.WristHealth", &wrist_health, "mvr_wristhealth",
             "setMotherWristHealth"),
    page_row(Section::HandsHaptics, "Enabled", "Hands.Haptics.Enabled", &haptics_enabled, "mvr_haptics",
             "setMotherHapticsEnabled"),
    page_row(Section::HandsHaptics, "Strength", "Hands.Haptics.Strength", &haptics_strength,
             "mvr_hapticsstrength", "setMotherHapticsStrength", CommandRule::Number, kHapticStrength),
    page_row(Section::HandsHaptics, "GameRumble", "Hands.Haptics.GameRumble", &haptics_game_rumble,
             "mvr_hapticsgamerumble", "setMotherHapticsGameRumble"),
    page_row(Section::HandsHaptics, "Damage", "Hands.Haptics.Damage", &haptics_damage, "mvr_hapticsdamage",
             "setMotherHapticsDamage"),
    page_row(Section::HandsHaptics, "Tracker", "Hands.Haptics.Tracker", &haptics_tracker,
             "mvr_hapticstracker", "setMotherHapticsTracker"),
    page_row(Section::HandsHaptics, "Firing", "Hands.Haptics.Firing", &haptics_firing, "mvr_hapticsfiring",
             "setMotherHapticsFiring"),
    page_row(Section::HandsHaptics, "Hacking", "Hands.Haptics.Hacking", &haptics_hacking,
             "mvr_hapticshacking", "setMotherHapticsHacking"),
    page_row(Section::Body, "Visible", "Body.Visible", &body_visible, "mvr_bodyvisible",
             "setMotherBodyVisible", CommandRule::Choice),
    page_row(Section::Body, "EyeForward", "Body.EyeForward", &eye_forward, "mvr_eyeforward",
             "setMotherEyeForward", CommandRule::Number, kEyeForward),
    page_row(Section::Body, "RoomScale", "Body.RoomScale", &room_scale, "mvr_roomscale",
             "setMotherRoomScale"),
    page_row(Section::Body, "SeatedEyeRaise", "Body.SeatedEyeRaise", &seated_eye_raise, "mvr_seatedeyeraise",
             "setMotherSeatedEyeRaise", CommandRule::Number, kSeatedEyeRaise),
    page_row(Section::Body, "RealCrouch", "Body.RealCrouch", &real_crouch, "mvr_realcrouch",
             "setMotherRealCrouch"),
    // v2 (spec 24 §10), file only: the Graphics page's items are shown only
    // once the user has confirmed them (24 §10), so these have no command
    // or push yet. Read at attach.
    SettingRow{Section::Graphics, "EngineSettingsOverride", "Graphics.EngineSettingsOverride",
               &engine_settings_override},
    SettingRow{Section::Graphics, "ShadowMap4096", "Graphics.ShadowMap4096", &shadow_map_4096},
    SettingRow{Section::Graphics, "SharpReflections", "Graphics.SharpReflections", &sharp_reflections},
    SettingRow{Section::Graphics, "StreamingTextureHeapMB", "Graphics.StreamingTextureHeapMB",
               &streaming_texture_heap_mb},
    SettingRow{Section::Graphics, "StreamingModelHeapMB", "Graphics.StreamingModelHeapMB",
               &streaming_model_heap_mb},
};

} // namespace

std::span<const SettingRow> settings_table() noexcept { return kRows; }

const SettingRow* find_command(std::string_view name) noexcept {
    const auto it = std::ranges::find_if(
        kRows, [name](const SettingRow& r) { return !r.command.empty() && r.command == name; });
    return it == kRows.end() ? nullptr : &*it;
}

std::string_view to_string(VRRuntime r) noexcept {
    switch (r) {
        case VRRuntime::None: return "None";
        case VRRuntime::Oculus: return "Oculus";
        case VRRuntime::OpenVR: return "OpenVR";
        case VRRuntime::OpenXR: return "OpenXR";
    }
    return "OpenVR";
}

std::string_view to_string(MirrorViewType m) noexcept {
    switch (m) {
        case MirrorViewType::BothEyes: return "BothEyes";
        case MirrorViewType::RightEye: return "RightEye";
        case MirrorViewType::LeftEye: return "LeftEye";
    }
    return "RightEye";
}

std::string_view to_string(StereoAA a) noexcept {
    switch (a) {
        case StereoAA::Auto: return "Auto";
        case StereoAA::On: return "On";
        case StereoAA::Off: return "Off";
    }
    return "Auto";
}

std::optional<StereoAA> parse_stereo_aa(std::string_view s) noexcept {
    if (s == "Auto") return StereoAA::Auto;
    if (s == "On") return StereoAA::On;
    if (s == "Off") return StereoAA::Off;
    return std::nullopt;
}

std::string_view to_string(PlayMode m) noexcept { return m == PlayMode::Standing ? "Standing" : "Seated"; }

std::string_view to_string(FlashlightAim a) noexcept {
    switch (a) {
        case FlashlightAim::Head: return "Head";
        case FlashlightAim::LeftHand: return "LeftHand";
        case FlashlightAim::RightHand: return "RightHand";
    }
    return "Head";
}

std::string_view to_string(BodyVisible v) noexcept {
    switch (v) {
        case BodyVisible::Full: return "Full";
        case BodyVisible::ArmsAndHands: return "ArmsAndHands";
        case BodyVisible::HandsOnly: return "HandsOnly";
        case BodyVisible::None: return "None";
    }
    return "Full";
}

// Case-sensitive, as spec 22 §3.1 and 21 §15 state.
std::optional<PlayMode> parse_play_mode(std::string_view s) noexcept {
    if (s == "Seated") return PlayMode::Seated;
    if (s == "Standing") return PlayMode::Standing;
    return std::nullopt;
}

std::optional<FlashlightAim> parse_flashlight_aim(std::string_view s) noexcept {
    if (s == "Head") return FlashlightAim::Head;
    if (s == "LeftHand") return FlashlightAim::LeftHand;
    if (s == "RightHand") return FlashlightAim::RightHand;
    return std::nullopt;
}

std::optional<BodyVisible> parse_body_visible(std::string_view s) noexcept {
    if (s == "Full") return BodyVisible::Full;
    if (s == "ArmsAndHands") return BodyVisible::ArmsAndHands;
    if (s == "HandsOnly") return BodyVisible::HandsOnly;
    if (s == "None") return BodyVisible::None;
    return std::nullopt;
}

std::optional<VRRuntime> parse_vr_runtime(std::string_view s) noexcept {
    if (s == "None" || s == "Disabled") return VRRuntime::None;
    if (s == "Oculus") return VRRuntime::Oculus;
    if (s == "OpenVR") return VRRuntime::OpenVR;
    if (s == "OpenXR") return VRRuntime::OpenXR;
    return std::nullopt;
}

std::optional<MirrorViewType> parse_mirror_view_type(std::string_view s) noexcept {
    if (s == "BothEyes") return MirrorViewType::BothEyes;
    if (s == "RightEye") return MirrorViewType::RightEye;
    if (s == "LeftEye") return MirrorViewType::LeftEye;
    return std::nullopt;
}

int snap_segments(int segments) noexcept {
    for (const int step : kSnapSegmentSteps) {
        if (segments >= step) return step;
    }
    return kSnapSegmentsDefault;
}

void sanitize(Settings& cfg) noexcept {
    // Oculus means OpenXR (spec 06 §1.1): Meta's PC runtime is an OpenXR
    // runtime and there is no LibOVR backend, so the runtime choice and the
    // next write see OpenXR.
    if (cfg.vr_runtime == VRRuntime::Oculus) cfg.vr_runtime = VRRuntime::OpenXR;

    // Written so that NaN, which compares false, is out of range too.
    float& scale = cfg.rendering.pixel_scale;
    if (!(scale >= kPixelScaleMin && scale <= kPixelScaleMax)) scale = kPixelScaleDefault;

    cfg.input.snap_rotation_segments = snap_segments(cfg.input.snap_rotation_segments);

    // 0 or [min, max]; NaN and negative speeds are 0 (spec 22 §3.1).
    float& speed = cfg.comfort.smooth_turn_speed;
    if (!(speed > 0.0f)) {
        speed = 0.0f;
    } else {
        speed = std::clamp(speed, kSmoothTurnSpeedMin, kSmoothTurnSpeedMax);
    }

    // 0 or [min, max]; a negative interval is the default.
    int& repeat = cfg.comfort.snap_turn_repeat_ms;
    if (repeat < 0) {
        repeat = kSnapTurnRepeatMsDefault;
    } else if (repeat > 0) {
        repeat = std::clamp(repeat, kSnapTurnRepeatMsMin, kSnapTurnRepeatMsMax);
    }

    // The rest of spec 22 §3.1 and 21 §15. Written so that NaN, which
    // compares false, takes the fallback too.
    const auto in_or = [](float& v, float lo, float hi, float fallback) {
        if (!(v >= lo && v <= hi)) v = fallback;
    };
    const auto clamped = [](float& v, float limit) {
        v = std::isnan(v) ? 0.0f : std::clamp(v, -limit, limit);
    };
    auto& comfort = cfg.comfort;
    in_or(comfort.vignette, 0.0f, kVignetteMax, 0.0f);
    if (comfort.standing_eye_height != 0.0f) {
        in_or(comfort.standing_eye_height, kStandingEyeHeightMin, kStandingEyeHeightMax, 0.0f);
    }
    in_or(comfort.world_scale, kWorldScaleMin, kWorldScaleMax, kWorldScaleDefault);
    in_or(cfg.hands.throw_strength, kThrowStrengthMin, kThrowStrengthMax, kThrowStrengthDefault);
    in_or(cfg.hands.smoothing, 0.0f, kHandSmoothingMax, 0.0f);
    in_or(cfg.hands.haptics.strength, 0.0f, kHapticStrengthMax, kHapticStrengthMax);
    clamped(cfg.body.eye_forward, kEyeForwardMax);
    clamped(cfg.body.seated_eye_raise, kSeatedEyeRaiseMax);

    // 0 (the game's) or [min, max]; a negative size is 0 (spec 24 §10).
    const auto heap = [](int& mb, int lo, int hi) { mb = mb <= 0 ? 0 : std::clamp(mb, lo, hi); };
    heap(cfg.graphics.streaming_texture_heap_mb, kTextureHeapMbMin, kTextureHeapMbMax);
    heap(cfg.graphics.streaming_model_heap_mb, kModelHeapMbMin, kModelHeapMbMax);
}

} // namespace omvr::core::config
