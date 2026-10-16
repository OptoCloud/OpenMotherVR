// OpenMotherVR-Provenance: clean
#include "platform/openxr/input.h"

#include "platform/openxr/constants.h"

#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace omvr::platform::openxr {

namespace {

using core::diag::ports::logf;

constexpr std::string_view kLogCreateActionFailed = "openxr input: xrCreateAction({}) failed: {}";
constexpr std::string_view kLogBindingsRejected   = "openxr input: bindings for {} rejected: {}";
constexpr std::string_view kLogActionSpaceFailed  = "openxr input: xrCreateActionSpace failed: {}";
constexpr std::string_view kLogGripNotLocated =
    "openxr input: {} grip not located: locate {} flags 0x{:X}; pose action {} active {}; trigger active {}";
constexpr std::string_view kLogActionSetFailed = "openxr input: xrCreateActionSet failed";
constexpr std::string_view kLogAttachFailed    = "openxr input: xrAttachSessionActionSets failed: {}";
constexpr std::string_view kLogAttached        = "openxr input: actions attached";
constexpr std::string_view kLogSync            = "openxr input: xrSyncActions -> {}";
constexpr std::string_view kLogNoProfile       = "openxr input: no controller profile bound";
constexpr std::string_view kLogProfile         = "openxr input: controller profile {}";

// The interaction profile of the Vive wands, the one touchpad family.
constexpr const char* kViveProfile = "htc/vive_controller";

const char* hand_name(int hand) noexcept { return hand == 0 ? "left" : "right"; }

} // namespace

XrPath Input::path(const char* s) noexcept {
    XrPath p = XR_NULL_PATH;
    xrStringToPath(instance_, s, &p);
    return p;
}

bool Input::make_action(XrAction& out, XrActionType type, const char* name, const char* localized) noexcept {
    XrActionCreateInfo ci{XR_TYPE_ACTION_CREATE_INFO};
    ci.actionType = type;
    strncpy_s(ci.actionName, name, _TRUNCATE);
    strncpy_s(ci.localizedActionName, localized, _TRUNCATE);
    ci.countSubactionPaths = static_cast<std::uint32_t>(hand_path_.size());
    ci.subactionPaths      = hand_path_.data();
    const XrResult r       = xrCreateAction(set_.get(), &ci, &out);
    if (XR_FAILED(r)) logf(log_, kLogCreateActionFailed, name, static_cast<int>(r));
    return XR_SUCCEEDED(r);
}

// The actions of 04 §5.8.4, in this order; the first failure stops.
bool Input::make_actions() noexcept {
    Actions& a = actions_;
    return make_action(a.grip_pose, XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Hand pose") &&
           make_action(a.trigger, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger") &&
           make_action(a.squeeze, XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Grip") &&
           make_action(a.stick, XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", "Thumbstick") &&
           make_action(a.stick_click, XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Thumbstick click") &&
           make_action(a.primary, XR_ACTION_TYPE_BOOLEAN_INPUT, "primary", "A / X") &&
           make_action(a.secondary, XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary", "B / Y") &&
           make_action(a.menu, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu") &&
           (!aim_poses_ || make_action(a.aim_pose, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Hand aim"));
}

void Input::suggest(const char* profile, const std::vector<Bind>& binds) noexcept {
    std::vector<XrActionSuggestedBinding> list;
    for (const auto& b : binds) {
        if (b.left) list.push_back({*b.action, path((std::string("/user/hand/left/") + b.left).c_str())});
        if (b.right) list.push_back({*b.action, path((std::string("/user/hand/right/") + b.right).c_str())});
    }
    XrInteractionProfileSuggestedBinding s{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    s.interactionProfile     = path(profile);
    s.countSuggestedBindings = static_cast<std::uint32_t>(list.size());
    s.suggestedBindings      = list.data();
    const XrResult r         = xrSuggestInteractionProfileBindings(instance_, &s);
    if (XR_FAILED(r)) logf(log_, kLogBindingsRejected, profile, static_cast<int>(r));
}

// The suggested bindings of 04 §5.8.4, one table per interaction profile.
void Input::suggest_all() noexcept {
    Actions& a = actions_;
    // The grip pose is the same on every profile, and so is the aim pose
    // (21 §16) when it is wanted.
    std::vector<Bind> common = {
        {&a.grip_pose, "input/grip/pose", "input/grip/pose"},
    };
    if (aim_poses_) common.push_back({&a.aim_pose, "input/aim/pose", "input/aim/pose"});
    auto with_common = [&](std::vector<Bind> v) {
        v.insert(v.end(), common.begin(), common.end());
        return v;
    };

    suggest("/interaction_profiles/oculus/touch_controller",
            with_common({
                {&a.trigger, "input/trigger/value", "input/trigger/value"},
                {&a.squeeze, "input/squeeze/value", "input/squeeze/value"},
                {&a.stick, "input/thumbstick", "input/thumbstick"},
                {&a.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
                {&a.primary, "input/x/click", "input/a/click"},
                {&a.secondary, "input/y/click", "input/b/click"},
                {&a.menu, "input/menu/click", nullptr},
            }));

    // Index has no menu button; the left trackpad press stands in for it.
    suggest("/interaction_profiles/valve/index_controller",
            with_common({
                {&a.trigger, "input/trigger/value", "input/trigger/value"},
                {&a.squeeze, "input/squeeze/value", "input/squeeze/value"},
                {&a.stick, "input/thumbstick", "input/thumbstick"},
                {&a.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
                {&a.primary, "input/a/click", "input/a/click"},
                {&a.secondary, "input/b/click", "input/b/click"},
                {&a.menu, "input/trackpad/force", nullptr},
            }));

    // Vive wands: the touchpad is the stick; face buttons come from touchpad
    // regions in the game mapping (ControllerFamily::Touchpad).
    suggest("/interaction_profiles/htc/vive_controller",
            with_common({
                {&a.trigger, "input/trigger/value", "input/trigger/value"},
                {&a.squeeze, "input/squeeze/click", "input/squeeze/click"},
                {&a.stick, "input/trackpad", "input/trackpad"},
                {&a.stick_click, "input/trackpad/click", "input/trackpad/click"},
                {&a.menu, "input/menu/click", "input/menu/click"},
            }));

    suggest("/interaction_profiles/microsoft/motion_controller",
            with_common({
                {&a.trigger, "input/trigger/value", "input/trigger/value"},
                {&a.squeeze, "input/squeeze/click", "input/squeeze/click"},
                {&a.stick, "input/thumbstick", "input/thumbstick"},
                {&a.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
                {&a.primary, "input/trackpad/click", "input/trackpad/click"},
                {&a.menu, "input/menu/click", "input/menu/click"},
            }));

    suggest("/interaction_profiles/khr/simple_controller",
            with_common({
                {&a.trigger, "input/select/click", "input/select/click"},
                {&a.menu, "input/menu/click", "input/menu/click"},
            }));
}

XrSpace Input::make_action_space(XrAction action, XrPath hand) noexcept {
    XrActionSpaceCreateInfo ci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    ci.action            = action;
    ci.subactionPath     = hand;
    ci.poseInActionSpace = kIdentityPose;
    XrSpace s            = XR_NULL_HANDLE;
    if (const XrResult r = xrCreateActionSpace(session_, &ci, &s); XR_FAILED(r))
        logf(log_, kLogActionSpaceFailed, static_cast<int>(r));
    return s;
}

XrActionStateGetInfo Input::get_info(XrAction a, int hand) noexcept {
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action        = a;
    gi.subactionPath = hand_path_[static_cast<std::size_t>(hand)];
    return gi;
}

float Input::get_float(XrAction a, int hand, bool& active) noexcept {
    XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};
    const auto gi = get_info(a, hand);
    if (XR_FAILED(xrGetActionStateFloat(session_, &gi, &s)) || !s.isActive) return 0.0f;
    active = true;
    return s.currentState;
}

bool Input::get_bool(XrAction a, int hand, bool& active) noexcept {
    XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};
    const auto gi = get_info(a, hand);
    if (XR_FAILED(xrGetActionStateBoolean(session_, &gi, &s)) || !s.isActive) return false;
    active = true;
    return s.currentState == XR_TRUE;
}

Vec2 Input::get_vec2(XrAction a, int hand, bool& active) noexcept {
    XrActionStateVector2f s{XR_TYPE_ACTION_STATE_VECTOR2F};
    const auto gi = get_info(a, hand);
    if (XR_FAILED(xrGetActionStateVector2f(session_, &gi, &s)) || !s.isActive) return {};
    active = true;
    return {s.currentState.x, s.currentState.y};
}

// Why a hand is untracked, once per hand and change of outcome (11 §3).
void Input::log_untracked_grip(int hand, XrResult located, XrSpaceLocationFlags flags) noexcept {
    XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};
    const XrActionStateGetInfo gi = get_info(actions_.grip_pose, hand);
    const XrResult pose_result    = xrGetActionStatePose(session_, &gi, &pose);
    XrActionStateFloat trigger{XR_TYPE_ACTION_STATE_FLOAT};
    const XrActionStateGetInfo ti = get_info(actions_.trigger, hand);
    xrGetActionStateFloat(session_, &ti, &trigger);
    const GripOutcome outcome{XR_FAILED(located), flags, pose.isActive != XR_FALSE,
                              trigger.isActive != XR_FALSE};
    if (!grip_logged_[static_cast<std::size_t>(hand)].changed(outcome)) return;
    logf(log_, kLogGripNotLocated, hand_name(hand), static_cast<int>(located), flags,
         static_cast<int>(pose_result), outcome.pose_active, outcome.trigger_active);
}

// The grip pose and, chained in the same call, its linear velocity (free:
// the runtime predicts with it anyway). Pose and velocity are tracked
// separately; an untracked pose leaves `s` with grip_valid false.
void Input::locate_grip(int hand, XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept {
    XrSpaceVelocity vel{XR_TYPE_SPACE_VELOCITY};
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION, &vel};
    const XrResult r =
        space == XR_NULL_HANDLE ? XR_ERROR_HANDLE_INVALID : xrLocateSpace(space, base, t, &loc);
    if (XR_FAILED(r) || (loc.locationFlags & kOrientationAndPosition) != kOrientationAndPosition) {
        log_untracked_grip(hand, r, loc.locationFlags);
        return;
    }
    s.grip       = from_xr(loc.pose);
    s.grip_valid = true;
    if (vel.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
        s.grip_velocity       = {vel.linearVelocity.x, vel.linearVelocity.y, vel.linearVelocity.z};
        s.grip_velocity_valid = true;
    }
}

// The aim pose (21 §5.2), located like the grip; untracked leaves aim_valid
// false, without a log line of its own (the grip's says why a hand is out).
void Input::locate_aim(XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept {
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (space == XR_NULL_HANDLE || XR_FAILED(xrLocateSpace(space, base, t, &loc)) ||
        (loc.locationFlags & kOrientationAndPosition) != kOrientationAndPosition) {
        return;
    }
    s.aim       = from_xr(loc.pose);
    s.aim_valid = true;
}

bool Input::create(XrInstance instance, XrSession session) noexcept {
    instance_  = instance;
    session_   = session;
    hand_path_ = {path("/user/hand/left"), path("/user/hand/right")};

    XrActionSetCreateInfo si{XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy_s(si.actionSetName, kActionSetName, _TRUNCATE);
    strncpy_s(si.localizedActionSetName, kActionSetLocalizedName, _TRUNCATE);
    if (XR_FAILED(xrCreateActionSet(instance_, &si, set_.put()))) {
        log_.line(kLogActionSetFailed);
        return false;
    }
    if (!make_actions()) {
        destroy();
        return false;
    }

    suggest_all();

    const XrActionSet set = set_.get();
    XrSessionActionSetsAttachInfo ai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    ai.countActionSets = 1;
    ai.actionSets      = &set;
    if (const XrResult r = xrAttachSessionActionSets(session_, &ai); XR_FAILED(r)) {
        logf(log_, kLogAttachFailed, static_cast<int>(r));
        destroy();
        return false;
    }

    for (std::size_t h = 0; h < grip_space_.size(); ++h) {
        grip_space_[h].reset(make_action_space(actions_.grip_pose, hand_path_[h]));
        if (aim_poses_) aim_space_[h].reset(make_action_space(actions_.aim_pose, hand_path_[h]));
    }
    log_.line(kLogAttached);
    return true;
}

void Input::destroy() noexcept {
    for (auto& s : grip_space_) s.reset();
    for (auto& s : aim_space_) s.reset();
    set_.reset(); // destroys its actions too
    actions_ = {};
    state_   = {};
    session_ = XR_NULL_HANDLE;
}

HandState Input::read_hand(int hand, XrSpace base, XrTime t) noexcept {
    const Actions& a = actions_;
    HandState s{};
    bool any      = false;
    s.trigger     = get_float(a.trigger, hand, any);
    s.squeeze     = get_float(a.squeeze, hand, any);
    s.stick       = get_vec2(a.stick, hand, any);
    s.stick_click = get_bool(a.stick_click, hand, any);
    s.primary     = get_bool(a.primary, hand, any);
    s.secondary   = get_bool(a.secondary, hand, any);
    s.menu        = get_bool(a.menu, hand, any);
    locate_grip(hand, grip_space_[static_cast<std::size_t>(hand)].get(), base, t, s);
    if (aim_poses_) locate_aim(aim_space_[static_cast<std::size_t>(hand)].get(), base, t, s);
    s.active = any || s.grip_valid;
    return s;
}

void Input::sync(XrTime time, XrSpace base) noexcept {
    if (!set_) return;

    const XrActiveActionSet active{set_.get(), XR_NULL_PATH};
    XrActionsSyncInfo si{XR_TYPE_ACTIONS_SYNC_INFO};
    si.countActiveActionSets = 1;
    si.activeActionSets      = &active;
    // XR_SESSION_NOT_FOCUSED (dashboard open) is a success code; states read
    // as inactive then, which zeroes the hands below.
    const XrResult synced = xrSyncActions(session_, &si);
    if (synced != sync_logged_) {
        sync_logged_ = synced;
        logf(log_, kLogSync, static_cast<int>(synced));
    }
    if (XR_FAILED(synced)) {
        state_.hands = {}; // nothing this frame, rather than last frame's poses
        return;
    }
    for (std::size_t h = 0; h < state_.hands.size(); ++h) {
        state_.hands[h] = read_hand(static_cast<int>(h), base, time);
    }
}

void Input::on_interaction_profile_changed() noexcept {
    if (session_ == XR_NULL_HANDLE) return;
    XrInteractionProfileState ps{XR_TYPE_INTERACTION_PROFILE_STATE};
    if (XR_FAILED(xrGetCurrentInteractionProfile(session_, hand_path_[1], &ps)) ||
        ps.interactionProfile == XR_NULL_PATH) {
        state_.family = ControllerFamily::Unknown;
        log_.line(kLogNoProfile);
        return;
    }
    char name[XR_MAX_PATH_LENGTH] = {};
    std::uint32_t n               = 0;
    xrPathToString(instance_, ps.interactionProfile, sizeof(name), &n, name);
    state_.family =
        std::strstr(name, kViveProfile) ? ControllerFamily::Touchpad : ControllerFamily::Thumbstick;
    logf(log_, kLogProfile, name);
}

} // namespace omvr::platform::openxr
