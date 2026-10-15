#include "platform/openvr/input_actions.h"

#include "platform/openvr/api_loader.h"
#include "platform/openvr/constants.h"
#include "platform/win32/text.h"

#include <openvr.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace omvr::platform::openvr {

namespace {

using core::diag::ports::logf;

constexpr std::string_view kLogNoInput = "openvr input: IVRInput unavailable, using legacy input";
constexpr std::string_view kLogManifestFailed =
    "openvr input: SetActionManifestPath failed ({}), using legacy input";
constexpr std::string_view kLogReady =
    "openvr input: SteamVR Input with {} actions + 2 hand poses, manifest {}";
constexpr std::string_view kLogException = "openvr input: {}";
constexpr std::string_view kLogPosesUnbound =
    "openvr input: hand pose actions unbound; a custom SteamVR binding for this game "
    "from an older version lacks them (reset it to the default binding)";

// The rows of a 3x4 pose, and the axes of a vector.
constexpr int kPoseRows    = 3;
constexpr int kPoseColumns = 4;
constexpr int kAxes        = 3;

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
    const auto bindings = default_bindings();
    for (const auto& [controller, file] : bindings) {
        if (!write_json(log_, dir / binding_file_name(controller), file)) return false;
    }
    return write_json(log_, manifest_path, manifest(bindings));
}

void Actions::register_actions(vr::IVRInput* input) {
    input->GetActionSetHandle(kSet, &set_);
    for (const auto& a : kActions) {
        input->GetActionHandle(action_path(a.name).c_str(), &handles_[index(a.id)]);
    }
    for (std::size_t h = 0; h < kPoseActions.size(); ++h) {
        input->GetActionHandle(action_path(kPoseActions[h]).c_str(), &pose_handles_[h]);
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
        logf(log_, kLogReady, kActions.size(), win32::utf8(manifest_path));
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

    // A bound turn_engage (touchpad click) means the touchpad scheme.
    vr::InputDigitalActionData_t engage{};
    input_->GetDigitalActionData(handles_[index(Action::TurnEngage)], &engage, sizeof(engage),
                                 vr::k_ulInvalidInputValueHandle);
    c.touchpad_scheme = engage.bActive;
    c.turn_engaged    = !engage.bActive || engage.bState;
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
        if (!d.pose.bPoseIsValid) continue;
        auto& p = out[h];
        for (int r = 0; r < kPoseRows; ++r)
            for (int c = 0; c < kPoseColumns; ++c) p.m[r][c] = d.pose.mDeviceToAbsoluteTracking.m[r][c];
        for (int i = 0; i < kAxes; ++i) p.velocity[i] = d.pose.vVelocity.v[i];
        p.valid = true;
    }
    if (unbound && !unbound_logged_) report_unbound_poses();
    return out;
}

// Controllers bound (fire reads active) but a hand pose not: a binding
// saved from a manifest before the hand poses existed. Said once.
void Actions::report_unbound_poses() noexcept {
    vr::InputAnalogActionData_t fire{};
    if (input_->GetAnalogActionData(handles_[index(Action::Fire)], &fire, sizeof(fire),
                                    vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
        fire.bActive) {
        unbound_logged_ = true;
        log_.line(kLogPosesUnbound);
    }
}

} // namespace omvr::platform::openvr
