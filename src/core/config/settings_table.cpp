#include "core/config/settings_table.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace omvr::core::config {

namespace {

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
// Spec 23 §8.
constexpr CommandRange kActivationDistance{kActivationDistanceMin, kActivationDistanceMax, true};
constexpr CommandRange kTerminalTextScale{kTerminalTextScales.front(), kTerminalTextScales.back()};
constexpr CommandRange kHudDistance{kHudDistanceMin, kHudDistanceMax};
constexpr CommandRange kHudWidth{kHudWidthMin, kHudWidthMax};
constexpr CommandRange kHudPitch{kHudPitchMin, kHudPitchMax};
// Spec 24 §10: 0 keeps the game's own size.
constexpr CommandRange kTextureHeap{static_cast<float>(kTextureHeapMbMin),
                                    static_cast<float>(kTextureHeapMbMax), true};
constexpr CommandRange kModelHeap{static_cast<float>(kModelHeapMbMin), static_cast<float>(kModelHeapMbMax),
                                  true};

// A row of the v2 pages: its command (named by this project, spec 22 §3.3)
// and the page's function its value is pushed to.
constexpr SettingRow page_row(Section section, std::string_view path, Field field, std::string_view command,
                              std::string_view push, CommandRule rule = CommandRule::Switch,
                              CommandRange range = {}) {
    return SettingRow{section, path, field, command, rule, CommandEffect::None, range, push};
}

// A 2D effect switch of the Graphics page (spec 24 §8, §10).
constexpr SettingRow post_fx_row(std::string_view path, Field field, std::string_view command,
                                 std::string_view push) {
    SettingRow row = page_row(Section::Graphics, path, field, command, push);
    row.effect     = CommandEffect::ApplyPostFx;
    return row;
}

// Key names are the original's (spec 06 §3); "StereoAntiAliasing" and the
// OpenXR runtime value are this project's.
constexpr std::array kRows{
    SettingRow{Section::Settings, "VRRuntime", &at<&Settings::vr_runtime>, "mvr_vrruntime",
               CommandRule::RuntimeEntry, CommandEffect::QuitGame},
    SettingRow{Section::Rendering, "Rendering.MirrorViewType",
               &at<&Settings::rendering, &RenderingSettings::mirror_view_type>, "mvr_mirrorview",
               CommandRule::Choice},
    SettingRow{Section::Rendering, "Rendering.SymmetricMirrorView",
               &at<&Settings::rendering, &RenderingSettings::symmetric_mirror_view>, "mvr_mirrorviewsym",
               CommandRule::Switch},
    SettingRow{Section::Rendering, "Rendering.PixelScale",
               &at<&Settings::rendering, &RenderingSettings::pixel_scale>, "mvr_pixelscale",
               CommandRule::PixelScale},
    SettingRow{Section::Rendering, "Rendering.ClippingBlinders",
               &at<&Settings::rendering, &RenderingSettings::clipping_blinders>, "mvr_clipblinding",
               CommandRule::Switch, CommandEffect::ApplyBlinders},
    SettingRow{Section::Rendering, "Rendering.StereoAntiAliasing",
               &at<&Settings::rendering, &RenderingSettings::stereo_aa>},
    SettingRow{Section::Input, "Input.SnapRotation", &at<&Settings::input, &InputSettings::snap_rotation>,
               "mvr_snaprotation", CommandRule::Switch},
    SettingRow{Section::Input, "Input.SnapRotationSegments",
               &at<&Settings::input, &InputSettings::snap_rotation_segments>, "mvr_snaprotationsegments",
               CommandRule::Segments},
    SettingRow{Section::Input, "Input.VRInputRelativeWalking",
               &at<&Settings::input, &InputSettings::vr_input_relative_walking>, "mvr_inputrelativewalking",
               CommandRule::Switch},
    SettingRow{Section::Input, "Input.VRInputStrafing",
               &at<&Settings::input, &InputSettings::vr_input_strafing>, "mvr_inputstrafing",
               CommandRule::Switch},
    SettingRow{Section::Input, "Input.ToggleGrips", &at<&Settings::input, &InputSettings::toggle_grips>,
               "mvr_togglegrips", CommandRule::Switch},
    SettingRow{Section::Input, "Input.ToggleCrouch", &at<&Settings::input, &InputSettings::toggle_crouch>,
               "mvr_togglecrouch", CommandRule::Switch},
    // "Dev. Debug Options" on the options page.
    SettingRow{Section::Settings, "DebugOptions", &at<&Settings::debug, &DebugSettings::debug>, "mvr_debug",
               CommandRule::Switch},
    SettingRow{Section::Settings, "SkipStartupIntro",
               &at<&Settings::debug, &DebugSettings::skip_startup_intro>, "mvr_skipintro",
               CommandRule::Switch},
    // Ours, file only (no options-page entry).
    SettingRow{Section::Settings, "DiscordRichPresence", &at<&Settings::discord_rich_presence>},
    // v2 (spec 22 §3.1, 21 §15): the pages Comfort, Controls, Hands and Body
    // (22 §12, 21 §15.3) send these commands and receive these pushes.
    page_row(Section::Comfort, "Comfort.SmoothTurnSpeed",
             &at<&Settings::comfort, &ComfortSettings::smooth_turn_speed>, "mvr_smoothturnspeed",
             "setMotherSmoothTurnSpeed", CommandRule::Number, kSmoothTurnSpeed),
    page_row(Section::Comfort, "Comfort.SnapTurnRepeatMs",
             &at<&Settings::comfort, &ComfortSettings::snap_turn_repeat_ms>, "mvr_snapturnrepeat",
             "setMotherSnapTurnRepeatMs", CommandRule::Integer, kSnapTurnRepeat),
    page_row(Section::Comfort, "Comfort.SnapTurnFade",
             &at<&Settings::comfort, &ComfortSettings::snap_turn_fade>, "mvr_snapturnfade",
             "setMotherSnapTurnFade"),
    page_row(Section::Comfort, "Comfort.Vignette", &at<&Settings::comfort, &ComfortSettings::vignette>,
             "mvr_vignette", "setMotherVignette", CommandRule::Number, kVignette),
    page_row(Section::Comfort, "Comfort.PlayMode", &at<&Settings::comfort, &ComfortSettings::play_mode>,
             "mvr_playmode", "setMotherPlayMode", CommandRule::Choice),
    page_row(Section::Comfort, "Comfort.StandingEyeHeight",
             &at<&Settings::comfort, &ComfortSettings::standing_eye_height>, "mvr_standingeyeheight",
             "setMotherStandingEyeHeight", CommandRule::Number, kStandingEyeHeight),
    page_row(Section::Comfort, "Comfort.WorldScale", &at<&Settings::comfort, &ComfortSettings::world_scale>,
             "mvr_worldscale", "setMotherWorldScale", CommandRule::Number, kWorldScale),
    page_row(Section::Controls, "Controls.InteractionAwareTurning",
             &at<&Settings::controls, &ControlsSettings::interaction_aware_turning>,
             "mvr_interactionawareturning", "setMotherInteractionAwareTurning"),
    page_row(Section::Controls, "Controls.DpadEmulation",
             &at<&Settings::controls, &ControlsSettings::dpad_emulation>, "mvr_dpademulation",
             "setMotherDpadEmulation"),
    page_row(Section::Controls, "Controls.RecenterGesture",
             &at<&Settings::controls, &ControlsSettings::recenter_gesture>, "mvr_recentergesture",
             "setMotherRecenterGesture"),
    page_row(Section::Controls, "Controls.HudToggleGesture",
             &at<&Settings::controls, &ControlsSettings::hud_toggle_gesture>, "mvr_hudtogglegesture",
             "setMotherHudToggleGesture"),
    page_row(Section::Controls, "Controls.HolsterGestures",
             &at<&Settings::controls, &ControlsSettings::holster_gestures>, "mvr_holstergestures",
             "setMotherHolsterGestures"),
    page_row(Section::Controls, "Controls.HidingLean",
             &at<&Settings::controls, &ControlsSettings::hiding_lean>, "mvr_hidinglean",
             "setMotherHidingLean"),
    page_row(Section::Controls, "Controls.HoldBreathGesture",
             &at<&Settings::controls, &ControlsSettings::hold_breath_gesture>, "mvr_holdbreathgesture",
             "setMotherHoldBreathGesture"),
    page_row(Section::Controls, "Controls.ControllerGlyphs",
             &at<&Settings::controls, &ControlsSettings::controller_glyphs>, "mvr_controllerglyphs",
             "setMotherControllerGlyphs"),
    page_row(Section::Controls, "Controls.VrOffHotkey",
             &at<&Settings::controls, &ControlsSettings::vr_off_hotkey>, "mvr_vroffhotkey",
             "setMotherVrOffHotkey"),
    page_row(Section::Controls, "Controls.CrashSafeLog",
             &at<&Settings::controls, &ControlsSettings::crash_safe_log>, "mvr_crashsafelog",
             "setMotherCrashSafeLog"),
    page_row(Section::Hands, "Hands.Enabled", &at<&Settings::hands, &HandsSettings::enabled>, "mvr_hands",
             "setMotherHandsEnabled"),
    page_row(Section::Hands, "Hands.AimFromHand", &at<&Settings::hands, &HandsSettings::aim_from_hand>,
             "mvr_aimfromhand", "setMotherAimFromHand"),
    page_row(Section::Hands, "Hands.TrackerInHand", &at<&Settings::hands, &HandsSettings::tracker_in_hand>,
             "mvr_trackerinhand", "setMotherTrackerInHand"),
    page_row(Section::Hands, "Hands.FlashlightAim", &at<&Settings::hands, &HandsSettings::flashlight_aim>,
             "mvr_flashlightaim", "setMotherFlashlightAim", CommandRule::Choice),
    page_row(Section::Hands, "Hands.MeleeByMotion", &at<&Settings::hands, &HandsSettings::melee_by_motion>,
             "mvr_meleebymotion", "setMotherMeleeByMotion"),
    page_row(Section::Hands, "Hands.ThrowByMotion", &at<&Settings::hands, &HandsSettings::throw_by_motion>,
             "mvr_throwbymotion", "setMotherThrowByMotion"),
    page_row(Section::Hands, "Hands.ThrowStrength", &at<&Settings::hands, &HandsSettings::throw_strength>,
             "mvr_throwstrength", "setMotherThrowStrength", CommandRule::Number, kThrowStrength),
    page_row(Section::Hands, "Hands.Smoothing", &at<&Settings::hands, &HandsSettings::smoothing>,
             "mvr_handsmoothing", "setMotherHandSmoothing", CommandRule::Number, kHandSmoothing),
    page_row(Section::Hands, "Hands.WristHealth", &at<&Settings::hands, &HandsSettings::wrist_health>,
             "mvr_wristhealth", "setMotherWristHealth"),
    page_row(Section::HandsHaptics, "Hands.Haptics.Enabled",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::enabled>, "mvr_haptics",
             "setMotherHapticsEnabled"),
    page_row(Section::HandsHaptics, "Hands.Haptics.Strength",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::strength>,
             "mvr_hapticsstrength", "setMotherHapticsStrength", CommandRule::Number, kHapticStrength),
    page_row(Section::HandsHaptics, "Hands.Haptics.GameRumble",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::game_rumble>,
             "mvr_hapticsgamerumble", "setMotherHapticsGameRumble"),
    page_row(Section::HandsHaptics, "Hands.Haptics.Damage",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::damage>, "mvr_hapticsdamage",
             "setMotherHapticsDamage"),
    page_row(Section::HandsHaptics, "Hands.Haptics.Tracker",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::tracker>, "mvr_hapticstracker",
             "setMotherHapticsTracker"),
    page_row(Section::HandsHaptics, "Hands.Haptics.Firing",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::firing>, "mvr_hapticsfiring",
             "setMotherHapticsFiring"),
    page_row(Section::HandsHaptics, "Hands.Haptics.Hacking",
             &at<&Settings::hands, &HandsSettings::haptics, &HapticsSettings::hacking>, "mvr_hapticshacking",
             "setMotherHapticsHacking"),
    page_row(Section::Body, "Body.Visible", &at<&Settings::body, &BodySettings::visible>, "mvr_bodyvisible",
             "setMotherBodyVisible", CommandRule::Choice),
    page_row(Section::Body, "Body.EyeForward", &at<&Settings::body, &BodySettings::eye_forward>,
             "mvr_eyeforward", "setMotherEyeForward", CommandRule::Number, kEyeForward),
    page_row(Section::Body, "Body.RoomScale", &at<&Settings::body, &BodySettings::room_scale>,
             "mvr_roomscale", "setMotherRoomScale"),
    page_row(Section::Body, "Body.SeatedEyeRaise", &at<&Settings::body, &BodySettings::seated_eye_raise>,
             "mvr_seatedeyeraise", "setMotherSeatedEyeRaise", CommandRule::Number, kSeatedEyeRaise),
    page_row(Section::Body, "Body.RealCrouch", &at<&Settings::body, &BodySettings::real_crouch>,
             "mvr_realcrouch", "setMotherRealCrouch"),
    // v2 (spec 24 §10): the Graphics page (user decision of 2026-10-18). The
    // engine settings are read at attach and apply at the next launch.
    page_row(Section::Graphics, "Graphics.EngineSettingsOverride",
             &at<&Settings::graphics, &GraphicsSettings::engine_settings_override>,
             "mvr_enginesettingsoverride", "setMotherEngineSettingsOverride"),
    page_row(Section::Graphics, "Graphics.ShadowMap4096",
             &at<&Settings::graphics, &GraphicsSettings::shadow_map_4096>, "mvr_shadowmap4096",
             "setMotherShadowMap4096"),
    page_row(Section::Graphics, "Graphics.SharpReflections",
             &at<&Settings::graphics, &GraphicsSettings::sharp_reflections>, "mvr_sharpreflections",
             "setMotherSharpReflections"),
    page_row(Section::Graphics, "Graphics.StreamingTextureHeapMB",
             &at<&Settings::graphics, &GraphicsSettings::streaming_texture_heap_mb>,
             "mvr_streamingtextureheap", "setMotherStreamingTextureHeapMB", CommandRule::Integer,
             kTextureHeap),
    page_row(Section::Graphics, "Graphics.StreamingModelHeapMB",
             &at<&Settings::graphics, &GraphicsSettings::streaming_model_heap_mb>, "mvr_streamingmodelheap",
             "setMotherStreamingModelHeapMB", CommandRule::Integer, kModelHeap),
    // 24 §8: a toggle's site is installed only for a key that is false at
    // attach; a change reaches that site at once (ApplyPostFx).
    post_fx_row("Graphics.LensFlares", &at<&Settings::graphics, &GraphicsSettings::lens_flares>,
                "mvr_lensflares", "setMotherLensFlares"),
    post_fx_row("Graphics.ChromaticAberration",
                &at<&Settings::graphics, &GraphicsSettings::chromatic_aberration>, "mvr_chromaticaberration",
                "setMotherChromaticAberration"),
    post_fx_row("Graphics.FilmGrain", &at<&Settings::graphics, &GraphicsSettings::film_grain>,
                "mvr_filmgrain", "setMotherFilmGrain"),
    // 24 §15, §16 (r11): no page item yet (24 §10: items 15 to 17 are not
    // shown), so file only. FullShadowDetail is the key alone: its feature
    // waits on 24 §12 item 14.
    SettingRow{Section::Graphics, "Graphics.ConstantSmokeLighting",
               &at<&Settings::graphics, &GraphicsSettings::constant_smoke_lighting>},
    SettingRow{Section::Graphics, "Graphics.ConstantSmokeBrightness",
               &at<&Settings::graphics, &GraphicsSettings::constant_smoke_brightness>},
    SettingRow{Section::Graphics, "Graphics.FullShadowDetail",
               &at<&Settings::graphics, &GraphicsSettings::full_shadow_detail>},
    // v2 (spec 23 §8): the Interaction and HUD pages. FastAnimations is
    // reserved (23 §4.6): file only, no page item.
    page_row(Section::Interaction, "Interaction.PhysicalLevers",
             &at<&Settings::interaction, &InteractionSettings::physical_levers>, "mvr_physicallevers",
             "setMotherPhysicalLevers"),
    page_row(Section::Interaction, "Interaction.CameraTakeovers",
             &at<&Settings::interaction, &InteractionSettings::camera_takeovers>, "mvr_cameratakeovers",
             "setMotherCameraTakeovers", CommandRule::Choice),
    SettingRow{Section::Interaction, "Interaction.FastAnimations",
               &at<&Settings::interaction, &InteractionSettings::fast_animations>},
    page_row(Section::Interaction, "Interaction.TerminalDistance",
             &at<&Settings::interaction, &InteractionSettings::terminal_distance>, "mvr_terminaldistance",
             "setMotherTerminalDistance", CommandRule::Number, kActivationDistance),
    page_row(Section::Interaction, "Interaction.RewireDistance",
             &at<&Settings::interaction, &InteractionSettings::rewire_distance>, "mvr_rewiredistance",
             "setMotherRewireDistance", CommandRule::Number, kActivationDistance),
    page_row(Section::Interaction, "Interaction.TerminalTextScale",
             &at<&Settings::interaction, &InteractionSettings::terminal_text_scale>, "mvr_terminaltextscale",
             "setMotherTerminalTextScale", CommandRule::Number, kTerminalTextScale),
    page_row(Section::Hud, "Hud.Crosshair", &at<&Settings::hud, &HudSettings::crosshair>, "mvr_crosshair",
             "setMotherCrosshair", CommandRule::Choice),
    page_row(Section::Hud, "Hud.Hidden", &at<&Settings::hud, &HudSettings::hidden>, "mvr_hidehud",
             "setMotherHudHidden"),
    page_row(Section::Hud, "Hud.ItemHighlight", &at<&Settings::hud, &HudSettings::item_highlight>,
             "mvr_itemhighlight", "setMotherItemHighlight"),
    page_row(Section::Hud, "Hud.Placement", &at<&Settings::hud, &HudSettings::placement>, "mvr_hudplacement",
             "setMotherHudPlacement", CommandRule::Choice),
    page_row(Section::Hud, "Hud.Distance", &at<&Settings::hud, &HudSettings::distance>, "mvr_huddistance",
             "setMotherHudDistance", CommandRule::Number, kHudDistance),
    page_row(Section::Hud, "Hud.Width", &at<&Settings::hud, &HudSettings::width>, "mvr_hudwidth",
             "setMotherHudWidth", CommandRule::Number, kHudWidth),
    page_row(Section::Hud, "Hud.Pitch", &at<&Settings::hud, &HudSettings::pitch>, "mvr_hudpitch",
             "setMotherHudPitch", CommandRule::Number, kHudPitch),
};

} // namespace

std::span<const SettingRow> settings_table() noexcept { return kRows; }

const SettingRow* find_command(std::string_view name) noexcept {
    const auto it = std::ranges::find_if(
        kRows, [name](const SettingRow& r) { return !r.command.empty() && r.command == name; });
    return it == kRows.end() ? nullptr : &*it;
}

float terminal_text_step(float scale) noexcept {
    if (std::isnan(scale)) return kTerminalTextScaleDefault;
    float best = kTerminalTextScales.front();
    for (const float step : kTerminalTextScales) {
        if (std::abs(scale - step) < std::abs(scale - best)) best = step;
    }
    return best;
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
    // Outside [0, 1]: the nearest bound; NaN: the default (spec 24 §10).
    float& smoke = cfg.graphics.constant_smoke_brightness;
    smoke        = std::isnan(smoke) ? kConstantSmokeBrightnessDefault
                                     : std::clamp(smoke, 0.0f, kConstantSmokeBrightnessMax);

    // Spec 23 §8: the activation distances are 0 or [min, max] (a value in
    // (0, min) becomes min); the text scale is the nearest step; the HUD
    // panel's values are clamped. NaN takes the default.
    const auto distance = [](float& d) {
        d = !(d > 0.0f) ? 0.0f : std::clamp(d, kActivationDistanceMin, kActivationDistanceMax);
    };
    distance(cfg.interaction.terminal_distance);
    distance(cfg.interaction.rewire_distance);
    cfg.interaction.terminal_text_scale = terminal_text_step(cfg.interaction.terminal_text_scale);

    const auto clamp_or = [](float& v, float lo, float hi, float fallback) {
        v = std::isnan(v) ? fallback : std::clamp(v, lo, hi);
    };
    clamp_or(cfg.hud.distance, kHudDistanceMin, kHudDistanceMax, kHudDistanceDefault);
    clamp_or(cfg.hud.width, kHudWidthMin, kHudWidthMax, kHudWidthDefault);
    clamp_or(cfg.hud.pitch, kHudPitchMin, kHudPitchMax, 0.0f);
}

} // namespace omvr::core::config
