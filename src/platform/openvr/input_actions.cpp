#include "platform/openvr/input_actions.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "platform/openvr/api_loader.h"
#include "platform/win32/text.h"

#include <openvr.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace omvr::platform::openvr {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using win32::utf8;
} // namespace diag

} // namespace

// ---- Runtime ----------------------------------------------------------------------

bool Actions::digital(Action a) noexcept {
    vr::InputDigitalActionData_t d{};
    return input_->GetDigitalActionData(handles_[index(a)], &d, sizeof(d), vr::k_ulInvalidInputValueHandle) ==
               vr::VRInputError_None && d.bActive && d.bState;
}

Actions::Analog Actions::analog(Action a) noexcept {
    vr::InputAnalogActionData_t d{};
    if (input_->GetAnalogActionData(handles_[index(a)], &d, sizeof(d), vr::k_ulInvalidInputValueHandle) !=
        vr::VRInputError_None || !d.bActive) {
        return {};
    }
    return {d.x, d.y};
}

bool Actions::init(const std::filesystem::path& dir) noexcept {
    try {
        auto* input = static_cast<vr::IVRInput*>(generic_interface(vr::IVRInput_Version));
        if (!input) {
            diag::log_line("openvr input: IVRInput unavailable, using legacy input");
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const auto bindings = default_bindings();
        for (const auto& [controller, file] : bindings) {
            if (!write_json(dir / ("bindings_" + controller + ".json"), file)) return false;
        }
        const auto manifest_path = dir / "actions.json";
        if (!write_json(manifest_path, manifest(bindings))) return false;

        // OpenVR takes UTF-8 paths.
        const auto err = input->SetActionManifestPath(diag::utf8(manifest_path).c_str());
        if (err != vr::VRInputError_None) {
            diag::logf("openvr input: SetActionManifestPath failed ({}), using legacy input", static_cast<int>(err));
            return false;
        }
        input->GetActionSetHandle(kSet, &set_);
        for (const auto& a : kActions) {
            input->GetActionHandle(action_path(a.name).c_str(), &handles_[index(a.id)]);
        }
        for (std::size_t h = 0; h < kPoseActions.size(); ++h) {
            input->GetActionHandle(action_path(kPoseActions[h]).c_str(), &pose_handles_[h]);
        }
        input_ = input;
        diag::logf("openvr input: SteamVR Input with {} actions + 2 hand poses, manifest {}", kActions.size(),
                   diag::utf8(manifest_path));
        return true;
    } catch (const std::exception& e) {
        diag::logf("openvr input: {}", e.what());
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
    c.move = {move.x, move.y};
    c.turn = {turn.x, turn.y};
    c.trigger_l = analog(Action::Crouch).x;
    c.trigger_r = analog(Action::Fire).x;
    c.grip_l = digital(Action::Tracker);
    c.grip_r = digital(Action::Aim);
    c.sprint = digital(Action::Sprint);
    c.interact = digital(Action::Interact);
    c.reload = digital(Action::Reload);
    c.radial = digital(Action::Radial);
    c.map = digital(Action::Map);
    c.flashlight = digital(Action::Flashlight);
    c.pause = digital(Action::Pause);

    // A bound turn_engage (touchpad click) means the touchpad scheme.
    vr::InputDigitalActionData_t engage{};
    input_->GetDigitalActionData(handles_[index(Action::TurnEngage)], &engage, sizeof(engage),
                                 vr::k_ulInvalidInputValueHandle);
    c.touchpad_scheme = engage.bActive;
    c.turn_engaged = !engage.bActive || engage.bState;
    return c;
}

std::array<HandPose, 2> Actions::hand_poses(int origin) noexcept {
    std::array<HandPose, 2> out{};
    if (!input_) return out;
    bool unbound = false;
    for (std::size_t h = 0; h < out.size(); ++h) {
        vr::InputPoseActionData_t d{};
        if (input_->GetPoseActionDataForNextFrame(pose_handles_[h], static_cast<vr::ETrackingUniverseOrigin>(origin),
                                                  &d, sizeof(d), vr::k_ulInvalidInputValueHandle) !=
            vr::VRInputError_None) {
            continue;
        }
        if (!d.bActive) {
            unbound = true;
            continue;
        }
        if (!d.pose.bPoseIsValid) continue;
        auto& p = out[h];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c) p.m[r][c] = d.pose.mDeviceToAbsoluteTracking.m[r][c];
        for (int i = 0; i < 3; ++i) p.velocity[i] = d.pose.vVelocity.v[i];
        p.valid = true;
    }
    // Controllers bound (fire reads active) but a hand pose not: a binding
    // saved from a manifest before the hand poses existed.
    if (unbound && !unbound_logged_) {
        vr::InputAnalogActionData_t fire{};
        if (input_->GetAnalogActionData(handles_[index(Action::Fire)], &fire, sizeof(fire),
                                        vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
            fire.bActive) {
            unbound_logged_ = true;
            diag::log_line("openvr input: hand pose actions unbound; a custom SteamVR binding for this game "
                           "from an older version lacks them (reset it to the default binding)");
        }
    }
    return out;
}

} // namespace omvr::platform::openvr
