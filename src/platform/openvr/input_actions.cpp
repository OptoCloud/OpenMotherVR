// OpenMotherVR-Provenance: clean
#include "platform/openvr/input_actions.h"

#include "core/hands/tuning.h"
#include "platform/openvr/api_loader.h"
#include "platform/openvr/constants.h"
#include "platform/win32/text.h"

#include <openvr.h>

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace omvr::platform::openvr {

namespace {

using core::diag::ports::logf;

constexpr std::string_view kLogNoInput = "openvr input: IVRInput unavailable, using legacy input";
constexpr std::string_view kLogManifestFailed =
    "openvr input: SetActionManifestPath failed ({}), using legacy input";
constexpr std::string_view kLogReady =
    "openvr input: SteamVR Input with {} actions + {} hand poses, manifest {}";
constexpr std::string_view kLogException = "openvr input: {}";
constexpr std::string_view kLogHapticsReady =
    "openvr input: haptic output actions {} and {} declared (21 §14.3)";
constexpr std::string_view kLogHapticsBound = "openvr input: haptic outputs bound: left {}, right {}";
constexpr std::string_view kLogHapticsUnbound =
    "openvr input: a haptic output is unbound (left {}, right {}); a custom SteamVR binding for this game "
    "from before the haptics lacks them (reset it to the default binding)";
constexpr std::string_view kLogAimUnbound =
    "openvr input: hand aim pose actions unbound; a custom SteamVR binding for this game "
    "from an older version lacks them (reset it to the default binding)";
constexpr std::string_view kLogPosesUnbound =
    "openvr input: hand pose actions unbound; a custom SteamVR binding for this game "
    "from an older version lacks them (reset it to the default binding)";

// Copies an OpenVR array into the same array type of a HandPose (the 3x4
// pose, float[3][4], or the velocity, float[3]); T is deduced from both.
template <class T>
void copy_same(T& dst, const T& src) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(&dst, &src, sizeof(T));
}

} // namespace

// ---- Runtime ----------------------------------------------------------------------

bool Actions::digital(Action a) noexcept {
    vr::InputDigitalActionData_t d{};
    return input_->GetDigitalActionData(handles_[index(a)], &d, sizeof(d), vr::k_ulInvalidInputValueHandle) ==
               vr::VRInputError_None &&
           d.bActive && d.bState;
}

Actions::Analog Actions::analog(Action a) noexcept {
    vr::InputAnalogActionData_t d{};
    if (input_->GetAnalogActionData(handles_[index(a)], &d, sizeof(d), vr::k_ulInvalidInputValueHandle) !=
            vr::VRInputError_None ||
        !d.bActive) {
        return {};
    }
    return {d.x, d.y};
}

// The binding files, then the manifest that names them (04 §5.8.2).
bool Actions::write_files(const std::filesystem::path& dir, const std::filesystem::path& manifest_path) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto bindings = default_bindings(haptics_);
    for (const auto& [controller, file] : bindings) {
        if (!write_json(log_, dir / binding_file_name(controller), file)) return false;
    }
    return write_json(log_, manifest_path, manifest(bindings, haptics_));
}

void Actions::register_actions(vr::IVRInput* input) {
    input->GetActionSetHandle(kSet, &set_);
    for (const auto& a : kActions) {
        input->GetActionHandle(action_path(a.name).c_str(), &handles_[index(a.id)]);
    }
    for (std::size_t h = 0; h < kPoseActions.size(); ++h) {
        input->GetActionHandle(action_path(kPoseActions[h]).c_str(), &pose_handles_[h]);
    }
    for (std::size_t h = 0; h < kAimPoseActions.size(); ++h) {
        input->GetActionHandle(action_path(kAimPoseActions[h]).c_str(), &aim_handles_[h]);
    }
    for (std::size_t h = 0; haptics_ && h < kHapticActions.size(); ++h) {
        input->GetActionHandle(output_path(kHapticActions[h]).c_str(), &haptic_handles_[h]);
    }
}

bool Actions::init(ApiLoader& api, const std::filesystem::path& dir) noexcept {
    try {
        const auto served = api.generic_interface(vr::IVRInput_Version);
        if (!served) {
            log_.line(kLogNoInput);
            return false;
        }
        auto* input              = static_cast<vr::IVRInput*>(*served);
        const auto manifest_path = dir / kManifestFileName;
        if (!write_files(dir, manifest_path)) return false;

        // OpenVR takes UTF-8 paths.
        const auto err = input->SetActionManifestPath(win32::utf8(manifest_path).c_str());
        if (err != vr::VRInputError_None) {
            logf(log_, kLogManifestFailed, static_cast<int>(err));
            return false;
        }
        register_actions(input);
        input_ = input;
        logf(log_, kLogReady, kActions.size(), kPoseActions.size() + kAimPoseActions.size(),
             win32::utf8(manifest_path));
        if (haptics_) logf(log_, kLogHapticsReady, kHapticActions[0], kHapticActions[1]);
        return true;
    } catch (const std::exception& e) {
        logf(log_, kLogException, e.what());
        return false;
    }
}

Controls Actions::poll() noexcept {
    Controls c;
    if (!input_) return c;
    vr::VRActiveActionSet_t active{};
    active.ulActionSet = set_;
    if (input_->UpdateActionState(&active, sizeof(active), 1) != vr::VRInputError_None) return c;

    const auto move = analog(Action::Move);
    const auto turn = analog(Action::Turn);
    c.move          = {move.x, move.y};
    c.turn          = {turn.x, turn.y};
    c.trigger_l     = analog(Action::Crouch).x;
    c.trigger_r     = analog(Action::Fire).x;
    c.grip_l        = digital(Action::Tracker);
    c.grip_r        = digital(Action::Aim);
    c.sprint        = digital(Action::Sprint);
    c.interact      = digital(Action::Interact);
    c.reload        = digital(Action::Reload);
    c.radial        = digital(Action::Radial);
    c.map           = digital(Action::Map);
    c.flashlight    = digital(Action::Flashlight);
    c.pause         = digital(Action::Pause);
    c.dpad          = {.up    = digital(Action::DpadUp),
                       .down  = digital(Action::DpadDown),
                       .left  = digital(Action::DpadLeft),
                       .right = digital(Action::DpadRight)};

    // A bound turn_engage (touchpad click) means the touchpad scheme.
    vr::InputDigitalActionData_t engage{};
    input_->GetDigitalActionData(handles_[index(Action::TurnEngage)], &engage, sizeof(engage),
                                 vr::k_ulInvalidInputValueHandle);
    c.touchpad_scheme = engage.bActive;
    c.turn_engaged    = !engage.bActive || engage.bState;
    if (haptics_ && !haptics_logged_) report_haptic_binding();
    return c;
}

std::array<HandPose, core::vr::kHandCount> Actions::hand_poses(int origin) noexcept {
    std::array<HandPose, core::vr::kHandCount> out{};
    if (!input_) return out;
    bool unbound = false;
    for (std::size_t h = 0; h < out.size(); ++h) {
        vr::InputPoseActionData_t d{};
        if (input_->GetPoseActionDataForNextFrame(
                pose_handles_[h], static_cast<vr::ETrackingUniverseOrigin>(origin), &d, sizeof(d),
                vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None) {
            continue;
        }
        if (!d.bActive) {
            unbound = true;
            continue;
        }
        // 21 §3.3: a pose SteamVR marks as running out of range counts as lost.
        if (!d.pose.bPoseIsValid || d.pose.eTrackingResult == vr::TrackingResult_Running_OutOfRange) continue;
        auto& p = out[h];
        copy_same(p.m, d.pose.mDeviceToAbsoluteTracking.m);
        copy_same(p.velocity, d.pose.vVelocity.v);
        p.valid = true;
        if (aim_poses_) read_aim(h, origin, p);
    }
    if (unbound && !unbound_logged_) report_unbound_poses();
    return out;
}

// The hand's aim pose with the same prediction as its grip (21 §5.2).
void Actions::read_aim(std::size_t hand, int origin, HandPose& p) noexcept {
    vr::InputPoseActionData_t d{};
    if (input_->GetPoseActionDataForNextFrame(aim_handles_[hand],
                                              static_cast<vr::ETrackingUniverseOrigin>(origin), &d, sizeof(d),
                                              vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None) {
        return;
    }
    // The grip is tracked, so the controller is bound: an inactive aim pose
    // is a binding without it.
    if (!d.bActive) {
        if (!aim_unbound_logged_) report_unbound_aim();
        return;
    }
    if (!d.pose.bPoseIsValid) return;
    copy_same(p.aim, d.pose.mDeviceToAbsoluteTracking.m);
    p.aim_valid = true;
}

void Actions::vibrate(std::size_t hand, float seconds, float amplitude) noexcept {
    if (!haptics_ready() || hand >= haptic_handles_.size()) return;
    input_->TriggerHapticVibrationAction(haptic_handles_[hand], 0.0f, seconds,
                                         core::hands::kVibrationFrequencyDefault, amplitude,
                                         vr::k_ulInvalidInputValueHandle);
}

// 21 §14.3, §18: whether each vibration output has a bound haptic device,
// once the controllers are bound (fire reads active). A saved binding from
// before the haptics leaves them unbound: said once, like the hand poses.
void Actions::report_haptic_binding() noexcept {
    if (!controllers_bound()) return;
    haptics_logged_ = true;
    std::array<bool, core::vr::kHandCount> bound{};
    for (std::size_t h = 0; h < bound.size(); ++h) {
        vr::VRInputValueHandle_t origins[vr::k_unMaxActionOriginCount]{};
        bound[h] = input_->GetActionOrigins(set_, haptic_handles_[h], origins,
                                            vr::k_unMaxActionOriginCount) == vr::VRInputError_None &&
                   origins[0] != vr::k_ulInvalidInputValueHandle;
    }
    if (bound[0] && bound[1])
        logf(log_, kLogHapticsBound, bound[0], bound[1]);
    else
        logf(log_, kLogHapticsUnbound, bound[0], bound[1]);
}

// 21 §16: a hand tracked by its grip pose but without an aim pose: a
// binding saved from a manifest before the aim poses were always declared.
// Said once, like the grip poses.
void Actions::report_unbound_aim() noexcept {
    aim_unbound_logged_ = true;
    log_.line(kLogAimUnbound);
}

// Controllers bound (fire reads active) but a hand pose not: a binding
// saved from a manifest before the hand poses existed. Said once.
void Actions::report_unbound_poses() noexcept {
    if (!controllers_bound()) return;
    unbound_logged_ = true;
    log_.line(kLogPosesUnbound);
}

// A binding is in use: the fire action reads active.
bool Actions::controllers_bound() noexcept {
    vr::InputAnalogActionData_t fire{};
    return input_->GetAnalogActionData(handles_[index(Action::Fire)], &fire, sizeof(fire),
                                       vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
           fire.bActive;
}

} // namespace omvr::platform::openvr
