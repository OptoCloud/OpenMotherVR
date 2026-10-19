// OpenMotherVR-Provenance: clean
#include "platform/openxr/input.h"

#include "platform/openxr/constants.h"

#include <array>
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
constexpr std::string_view kLogHapticCreated =
    "openxr input: haptic output action created for both hands (21 §14.3)";
constexpr std::string_view kLogHapticBound   = "openxr input: haptic output bound to {} source(s)";
constexpr std::string_view kLogHapticUnbound = "openxr input: haptic output not bound by this profile ({})";
constexpr std::string_view kLogAimUnbound =
    "openxr input: {} hand aim pose not bound by the current profile; aiming stays on the head";
constexpr std::string_view kLogFrameProfile = "openxr input: Steam Frame controller bindings suggested";

constexpr float kNanosecondsPerSecond = 1e9f;

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
    if (XR_FAILED(r)) {
        out = XR_NULL_HANDLE; // what a failed create left is not a handle
        logf(log_, kLogCreateActionFailed, name, static_cast<int>(r));
    }
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
           make_action(a.dpad_up, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_up", "D-pad up") &&
           make_action(a.dpad_down, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_down", "D-pad down") &&
           make_action(a.dpad_left, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_left", "D-pad left") &&
           make_action(a.dpad_right, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_right", "D-pad right") &&
           make_action(a.aim_pose, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Hand aim") &&
           (!haptics_ || make_action(a.haptic, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Vibration"));
}

bool Input::suggest(const char* profile, const std::vector<Bind>& binds) noexcept {
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
    return XR_SUCCEEDED(r);
}

// The suggested bindings of 04 §5.8.4, one table per interaction profile.
void Input::suggest_all() noexcept {
    Actions& a = actions_;
    // The grip pose is the same on every profile, and so is the aim pose
    // (21 §16), suggested whatever Hands.Enabled is so that it applies at
    // once (21 §15.1).
    std::vector<Bind> common = {
        {&a.grip_pose, "input/grip/pose", "input/grip/pose"},
        {&a.aim_pose, "input/aim/pose", "input/aim/pose"},
    };
    // 21 §14.3: every profile here has a haptic output on each hand.
    if (haptics_) common.push_back({&a.haptic, "output/haptic", "output/haptic"});
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
    // regions in the game mapping (ControlScheme::Touchpad).
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

    if (frame_profile_) suggest_frame(common);
}

// Steam Frame (Valve's public input documentation; only with its
// extension): the Touch layout where the buttons exist (right A interact,
// right B reload, the right Menu pauses), the left d-pad as the d-pad (22
// §7), Map on the left View and Flashlight on the left bumper, where Touch
// has its left X / Y.
void Input::suggest_frame(const std::vector<Bind>& common) noexcept {
    Actions& a             = actions_;
    std::vector<Bind> list = {
        {&a.trigger, "input/trigger/value", "input/trigger/value"},
        {&a.squeeze, "input/squeeze/value", "input/squeeze/value"},
        {&a.stick, "input/thumbstick", "input/thumbstick"},
        {&a.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
        {&a.primary, "input/view/click", "input/a/click"},
        {&a.secondary, "input/bumper/click", "input/b/click"},
        {&a.menu, nullptr, "input/menu/click"},
        {&a.dpad_up, "input/dpad_up/click", nullptr},
        {&a.dpad_down, "input/dpad_down/click", nullptr},
        {&a.dpad_left, "input/dpad_left/click", nullptr},
        {&a.dpad_right, "input/dpad_right/click", nullptr},
    };
    list.insert(list.end(), common.begin(), common.end());
    if (suggest(kFrameControllerProfile, list)) log_.line(kLogFrameProfile);
}

XrSpace Input::make_action_space(XrAction action, XrPath hand) noexcept {
    XrActionSpaceCreateInfo ci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    ci.action            = action;
    ci.subactionPath     = hand;
    ci.poseInActionSpace = kIdentityPose;
    XrSpace s            = XR_NULL_HANDLE;
    if (const XrResult r = xrCreateActionSpace(session_, &ci, &s); XR_FAILED(r)) {
        logf(log_, kLogActionSpaceFailed, static_cast<int>(r));
        return XR_NULL_HANDLE; // what a failed create left is not a handle
    }
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
// false, without a log line of its own (the grip's says why a hand is out),
// except a profile that binds no aim pose while the grip is tracked.
void Input::locate_aim(int hand, XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept {
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (space == XR_NULL_HANDLE || XR_FAILED(xrLocateSpace(space, base, t, &loc)) ||
        (loc.locationFlags & kOrientationAndPosition) != kOrientationAndPosition) {
        if (s.grip_valid) log_unbound_aim(hand);
        return;
    }
    s.aim       = from_xr(loc.pose);
    s.aim_valid = true;
}

// 21 §16: the aim pose action reads inactive on a tracked hand: the bound
// profile has no aim pose. Once per hand and process.
void Input::log_unbound_aim(int hand) noexcept {
    auto& logged = aim_unbound_logged_[static_cast<std::size_t>(hand)];
    if (logged) return;
    XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};
    const XrActionStateGetInfo gi = get_info(actions_.aim_pose, hand);
    if (XR_FAILED(xrGetActionStatePose(session_, &gi, &pose)) || pose.isActive != XR_FALSE) return;
    logged = true;
    logf(log_, kLogAimUnbound, hand_name(hand));
}

bool Input::create(XrInstance instance, XrSession session) noexcept {
    instance_  = instance;
    session_   = session;
    hand_path_ = {path("/user/hand/left"), path("/user/hand/right")};

    XrActionSetCreateInfo si{XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy_s(si.actionSetName, kActionSetName, _TRUNCATE);
    strncpy_s(si.localizedActionSetName, kActionSetLocalizedName, _TRUNCATE);
    if (XR_FAILED(xrCreateActionSet(instance_, &si, set_.put()))) {
        set_.release(); // nothing was created
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
        aim_space_[h].reset(make_action_space(actions_.aim_pose, hand_path_[h]));
    }
    log_.line(kLogAttached);
    if (haptics_) log_.line(kLogHapticCreated);
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
    s.dpad        = {.up    = get_bool(a.dpad_up, hand, any),
                     .down  = get_bool(a.dpad_down, hand, any),
                     .left  = get_bool(a.dpad_left, hand, any),
                     .right = get_bool(a.dpad_right, hand, any)};
    locate_grip(hand, grip_space_[static_cast<std::size_t>(hand)].get(), base, t, s);
    if (aim_poses_) locate_aim(hand, aim_space_[static_cast<std::size_t>(hand)].get(), base, t, s);
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
    state_.focused = synced == XR_SUCCESS;
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
    // 22 §10.1: each hand's profile picks that hand's glyphs.
    static_assert(kProfileNameSize == XR_MAX_PATH_LENGTH);
    std::array<bool, core::vr::kHandCount> bound{}; // a profile path, even one that could not be named
    for (std::size_t h = 0; h < hand_path_.size(); ++h) {
        auto& name = state_.profiles[h];
        name.fill('\0');
        XrInteractionProfileState hp{XR_TYPE_INTERACTION_PROFILE_STATE};
        std::uint32_t len = 0;
        bound[h]          = XR_SUCCEEDED(xrGetCurrentInteractionProfile(session_, hand_path_[h], &hp)) &&
                            hp.interactionProfile != XR_NULL_PATH;
        if (!bound[h] ||
            XR_FAILED(xrPathToString(instance_, hp.interactionProfile,
                                     static_cast<std::uint32_t>(name.size()), &len, name.data())))
            name.fill('\0');
    }
    ++state_.profile_generation;
    // The right hand's profile picks the family (04 §5.8.4).
    if (!bound[1]) {
        state_.family.reset();
        log_.line(kLogNoProfile);
        return;
    }
    const char* name = state_.profiles[1].data();
    using core::input::ControlScheme;
    state_.family = std::strstr(name, kViveProfile)                   ? ControlScheme::Touchpad
                    : std::strcmp(name, kFrameControllerProfile) == 0 ? ControlScheme::Frame
                                                                      : ControlScheme::Thumbstick;
    logf(log_, kLogProfile, name);
    log_haptic_binding();
}

// 21 §18: whether the profile now in use binds the haptic output.
void Input::log_haptic_binding() noexcept {
    if (actions_.haptic == XR_NULL_HANDLE) return;
    XrBoundSourcesForActionEnumerateInfo info{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
    info.action         = actions_.haptic;
    std::uint32_t count = 0;
    const XrResult r    = xrEnumerateBoundSourcesForAction(session_, &info, 0, &count, nullptr);
    if (XR_FAILED(r) || count == 0) {
        logf(log_, kLogHapticUnbound, static_cast<int>(r));
        return;
    }
    logf(log_, kLogHapticBound, count);
}

void Input::play_haptics(const core::vr::ports::HapticFrame& frame) noexcept {
    if (actions_.haptic == XR_NULL_HANDLE) return;
    for (std::size_t h = 0; h < frame.size(); ++h) {
        const auto& out = frame[h];
        XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
        info.action        = actions_.haptic;
        info.subactionPath = hand_path_[h];
        if (out.play) {
            XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION};
            v.duration  = static_cast<XrDuration>(out.seconds * kNanosecondsPerSecond);
            v.frequency = XR_FREQUENCY_UNSPECIFIED;
            v.amplitude = out.amplitude;
            xrApplyHapticFeedback(session_, &info, reinterpret_cast<const XrHapticBaseHeader*>(&v));
        } else if (out.stop) {
            xrStopHapticFeedback(session_, &info);
        }
    }
}

} // namespace omvr::platform::openxr
