#include "openxr_input.h"

#include "diag/log.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace mothervr::runtime::openxr::input {

namespace {

XrInstance g_instance = XR_NULL_HANDLE;
XrSession  g_session  = XR_NULL_HANDLE;
XrActionSet g_set     = XR_NULL_HANDLE;

std::array<XrPath, 2> g_hand_path{};

struct Actions {
    XrAction grip_pose   = XR_NULL_HANDLE;
    XrAction trigger     = XR_NULL_HANDLE;
    XrAction squeeze     = XR_NULL_HANDLE;
    XrAction stick       = XR_NULL_HANDLE;
    XrAction stick_click = XR_NULL_HANDLE;
    XrAction primary     = XR_NULL_HANDLE;
    XrAction secondary   = XR_NULL_HANDLE;
    XrAction menu        = XR_NULL_HANDLE;
} g_act;

std::array<XrSpace, 2> g_grip_space{XR_NULL_HANDLE, XR_NULL_HANDLE};

ControllerState g_state{};

XrPath path(const char* s) noexcept {
    XrPath p = XR_NULL_PATH;
    xrStringToPath(g_instance, s, &p);
    return p;
}

bool make_action(XrAction& out, XrActionType type, const char* name, const char* localized) noexcept {
    XrActionCreateInfo ci{XR_TYPE_ACTION_CREATE_INFO};
    ci.actionType = type;
    strncpy_s(ci.actionName, name, _TRUNCATE);
    strncpy_s(ci.localizedActionName, localized, _TRUNCATE);
    ci.countSubactionPaths = static_cast<std::uint32_t>(g_hand_path.size());
    ci.subactionPaths      = g_hand_path.data();
    const XrResult r = xrCreateAction(g_set, &ci, &out);
    if (XR_FAILED(r)) diag::logf("openxr input: xrCreateAction({}) failed: {}", name, static_cast<int>(r));
    return XR_SUCCEEDED(r);
}

// One binding: action + component path under /user/hand/<side>/input or /output.
struct Bind {
    XrAction*   action;
    const char* left;  // nullptr: not bound on that hand
    const char* right;
};

void suggest(const char* profile, const std::vector<Bind>& binds) noexcept {
    std::vector<XrActionSuggestedBinding> list;
    for (const auto& b : binds) {
        if (b.left)  list.push_back({*b.action, path((std::string("/user/hand/left/")  + b.left).c_str())});
        if (b.right) list.push_back({*b.action, path((std::string("/user/hand/right/") + b.right).c_str())});
    }
    XrInteractionProfileSuggestedBinding s{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    s.interactionProfile     = path(profile);
    s.countSuggestedBindings = static_cast<std::uint32_t>(list.size());
    s.suggestedBindings      = list.data();
    const XrResult r = xrSuggestInteractionProfileBindings(g_instance, &s);
    if (XR_FAILED(r)) diag::logf("openxr input: bindings for {} rejected: {}", profile, static_cast<int>(r));
}

void suggest_all() noexcept {
    // The grip pose is the same on every profile.
    const std::vector<Bind> common = {
        {&g_act.grip_pose, "input/grip/pose", "input/grip/pose"},
    };
    auto with_common = [&](std::vector<Bind> v) {
        v.insert(v.end(), common.begin(), common.end());
        return v;
    };

    suggest("/interaction_profiles/oculus/touch_controller", with_common({
        {&g_act.trigger,     "input/trigger/value",    "input/trigger/value"},
        {&g_act.squeeze,     "input/squeeze/value",    "input/squeeze/value"},
        {&g_act.stick,       "input/thumbstick",       "input/thumbstick"},
        {&g_act.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
        {&g_act.primary,     "input/x/click",          "input/a/click"},
        {&g_act.secondary,   "input/y/click",          "input/b/click"},
        {&g_act.menu,        "input/menu/click",       nullptr},
    }));

    // Index has no menu button; the left trackpad press stands in for it.
    suggest("/interaction_profiles/valve/index_controller", with_common({
        {&g_act.trigger,     "input/trigger/value",    "input/trigger/value"},
        {&g_act.squeeze,     "input/squeeze/value",    "input/squeeze/value"},
        {&g_act.stick,       "input/thumbstick",       "input/thumbstick"},
        {&g_act.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
        {&g_act.primary,     "input/a/click",          "input/a/click"},
        {&g_act.secondary,   "input/b/click",          "input/b/click"},
        {&g_act.menu,        "input/trackpad/force",   nullptr},
    }));

    // Vive wands: the touchpad is the stick; face buttons come from touchpad
    // regions in the game mapping (ControllerFamily::Touchpad).
    suggest("/interaction_profiles/htc/vive_controller", with_common({
        {&g_act.trigger,     "input/trigger/value",  "input/trigger/value"},
        {&g_act.squeeze,     "input/squeeze/click",  "input/squeeze/click"},
        {&g_act.stick,       "input/trackpad",       "input/trackpad"},
        {&g_act.stick_click, "input/trackpad/click", "input/trackpad/click"},
        {&g_act.menu,        "input/menu/click",     "input/menu/click"},
    }));

    suggest("/interaction_profiles/microsoft/motion_controller", with_common({
        {&g_act.trigger,     "input/trigger/value",    "input/trigger/value"},
        {&g_act.squeeze,     "input/squeeze/click",    "input/squeeze/click"},
        {&g_act.stick,       "input/thumbstick",       "input/thumbstick"},
        {&g_act.stick_click, "input/thumbstick/click", "input/thumbstick/click"},
        {&g_act.primary,     "input/trackpad/click",   "input/trackpad/click"},
        {&g_act.menu,        "input/menu/click",       "input/menu/click"},
    }));

    suggest("/interaction_profiles/khr/simple_controller", with_common({
        {&g_act.trigger, "input/select/click", "input/select/click"},
        {&g_act.menu,    "input/menu/click",   "input/menu/click"},
    }));
}

XrSpace make_action_space(XrAction action, XrPath hand) noexcept {
    XrActionSpaceCreateInfo ci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    ci.action            = action;
    ci.subactionPath     = hand;
    ci.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};
    XrSpace s = XR_NULL_HANDLE;
    if (const XrResult r = xrCreateActionSpace(g_session, &ci, &s); XR_FAILED(r))
        diag::logf("openxr input: xrCreateActionSpace failed: {}", static_cast<int>(r));
    return s;
}

XrActionStateGetInfo get_info(XrAction a, int hand) noexcept {
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action        = a;
    gi.subactionPath = g_hand_path[hand];
    return gi;
}

float get_float(XrAction a, int hand, bool& active) noexcept {
    XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};
    const auto gi = get_info(a, hand);
    if (XR_FAILED(xrGetActionStateFloat(g_session, &gi, &s)) || !s.isActive) return 0.0f;
    active = true;
    return s.currentState;
}

bool get_bool(XrAction a, int hand, bool& active) noexcept {
    XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};
    const auto gi = get_info(a, hand);
    if (XR_FAILED(xrGetActionStateBoolean(g_session, &gi, &s)) || !s.isActive) return false;
    active = true;
    return s.currentState == XR_TRUE;
}

Vec2 get_vec2(XrAction a, int hand, bool& active) noexcept {
    XrActionStateVector2f s{XR_TYPE_ACTION_STATE_VECTOR2F};
    const auto gi = get_info(a, hand);
    if (XR_FAILED(xrGetActionStateVector2f(g_session, &gi, &s)) || !s.isActive) return {};
    active = true;
    return {s.currentState.x, s.currentState.y};
}

// The grip pose and, chained in the same call, its linear velocity (free:
// the runtime predicts with it anyway). Pose and velocity are tracked
// separately; an untracked pose leaves `s` with grip_valid false.
void locate_grip(int hand, XrSpace space, XrSpace base, XrTime t, HandState& s) noexcept {
    XrSpaceVelocity vel{XR_TYPE_SPACE_VELOCITY};
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION, &vel};
    const XrResult r = space == XR_NULL_HANDLE ? XR_ERROR_HANDLE_INVALID : xrLocateSpace(space, base, t, &loc);
    constexpr XrSpaceLocationFlags kValid =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    // Why a hand is untracked, once per hand and change of outcome.
    if (XR_FAILED(r) || (loc.locationFlags & kValid) != kValid) {
        XrActionStatePose active{XR_TYPE_ACTION_STATE_POSE};
        const XrActionStateGetInfo gi = get_info(g_act.grip_pose, hand);
        const XrResult ar = xrGetActionStatePose(g_session, &gi, &active);
        XrActionStateFloat trig{XR_TYPE_ACTION_STATE_FLOAT};
        const XrActionStateGetInfo ti = get_info(g_act.trigger, hand);
        xrGetActionStateFloat(g_session, &ti, &trig);
        static std::uint64_t logged[2] = {~0ull, ~0ull};
        const std::uint64_t outcome = (XR_FAILED(r) ? 1ull : 0ull) | (loc.locationFlags << 1) |
                                      (std::uint64_t{active.isActive != XR_FALSE} << 40) |
                                      (std::uint64_t{trig.isActive != XR_FALSE} << 41);
        if (outcome != logged[hand]) {
            logged[hand] = outcome;
            diag::logf("openxr input: {} grip not located: locate {} flags 0x{:X}; pose action {} active {}; "
                       "trigger active {}",
                       hand == 0 ? "left" : "right", static_cast<int>(r), loc.locationFlags,
                       static_cast<int>(ar), active.isActive != XR_FALSE, trig.isActive != XR_FALSE);
        }
    }
    if (XR_FAILED(r) || (loc.locationFlags & kValid) != kValid) return;
    s.grip       = from_xr(loc.pose);
    s.grip_valid = true;
    if (vel.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
        s.grip_velocity       = {vel.linearVelocity.x, vel.linearVelocity.y, vel.linearVelocity.z};
        s.grip_velocity_valid = true;
    }
}

} // namespace

bool create(XrInstance instance, XrSession session) noexcept {
    g_instance = instance;
    g_session  = session;
    g_hand_path = {path("/user/hand/left"), path("/user/hand/right")};

    XrActionSetCreateInfo si{XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy_s(si.actionSetName, "gameplay", _TRUNCATE);
    strncpy_s(si.localizedActionSetName, "Gameplay", _TRUNCATE);
    if (XR_FAILED(xrCreateActionSet(g_instance, &si, &g_set))) {
        diag::log_line("openxr input: xrCreateActionSet failed");
        return false;
    }

    const bool ok =
        make_action(g_act.grip_pose,   XR_ACTION_TYPE_POSE_INPUT,       "grip_pose",   "Hand pose") &&
        make_action(g_act.trigger,     XR_ACTION_TYPE_FLOAT_INPUT,      "trigger",     "Trigger") &&
        make_action(g_act.squeeze,     XR_ACTION_TYPE_FLOAT_INPUT,      "squeeze",     "Grip") &&
        make_action(g_act.stick,       XR_ACTION_TYPE_VECTOR2F_INPUT,   "stick",       "Thumbstick") &&
        make_action(g_act.stick_click, XR_ACTION_TYPE_BOOLEAN_INPUT,    "stick_click", "Thumbstick click") &&
        make_action(g_act.primary,     XR_ACTION_TYPE_BOOLEAN_INPUT,    "primary",     "A / X") &&
        make_action(g_act.secondary,   XR_ACTION_TYPE_BOOLEAN_INPUT,    "secondary",   "B / Y") &&
        make_action(g_act.menu,        XR_ACTION_TYPE_BOOLEAN_INPUT,    "menu",        "Menu");
    if (!ok) {
        destroy();
        return false;
    }

    suggest_all();

    XrSessionActionSetsAttachInfo ai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    ai.countActionSets = 1;
    ai.actionSets      = &g_set;
    if (const XrResult r = xrAttachSessionActionSets(g_session, &ai); XR_FAILED(r)) {
        diag::logf("openxr input: xrAttachSessionActionSets failed: {}", static_cast<int>(r));
        destroy();
        return false;
    }

    for (int h = 0; h < 2; ++h) g_grip_space[h] = make_action_space(g_act.grip_pose, g_hand_path[h]);
    diag::log_line("openxr input: actions attached");
    return true;
}

void destroy() noexcept {
    for (auto& s : g_grip_space) {
        if (s != XR_NULL_HANDLE) xrDestroySpace(s);
        s = XR_NULL_HANDLE;
    }
    if (g_set != XR_NULL_HANDLE) xrDestroyActionSet(g_set); // destroys its actions too
    g_set     = XR_NULL_HANDLE;
    g_act     = {};
    g_state   = {};
    g_session = XR_NULL_HANDLE;
}

void sync(XrTime time, XrSpace base) noexcept {
    if (g_set == XR_NULL_HANDLE) return;

    XrActiveActionSet active{g_set, XR_NULL_PATH};
    XrActionsSyncInfo si{XR_TYPE_ACTIONS_SYNC_INFO};
    si.countActiveActionSets = 1;
    si.activeActionSets      = &active;
    // XR_SESSION_NOT_FOCUSED (dashboard open) is a success code; states read
    // as inactive then, which zeroes the hands below.
    const XrResult synced = xrSyncActions(g_session, &si);
    static XrResult logged_sync = XR_SUCCESS;
    if (synced != logged_sync) {
        logged_sync = synced;
        diag::logf("openxr input: xrSyncActions -> {}", static_cast<int>(synced));
    }
    if (XR_FAILED(synced)) {
        g_state.hands = {};   // nothing this frame, rather than last frame's poses
        return;
    }

    for (int h = 0; h < 2; ++h) {
        HandState s{};
        bool any = false;
        s.trigger     = get_float(g_act.trigger, h, any);
        s.squeeze     = get_float(g_act.squeeze, h, any);
        s.stick       = get_vec2 (g_act.stick, h, any);
        s.stick_click = get_bool (g_act.stick_click, h, any);
        s.primary     = get_bool (g_act.primary, h, any);
        s.secondary   = get_bool (g_act.secondary, h, any);
        s.menu        = get_bool (g_act.menu, h, any);
        locate_grip(h, g_grip_space[h], base, time, s);
        s.active      = any || s.grip_valid;
        g_state.hands[h] = s;
    }
}

void on_interaction_profile_changed() noexcept {
    if (g_session == XR_NULL_HANDLE) return;
    XrInteractionProfileState ps{XR_TYPE_INTERACTION_PROFILE_STATE};
    if (XR_FAILED(xrGetCurrentInteractionProfile(g_session, g_hand_path[1], &ps)) ||
        ps.interactionProfile == XR_NULL_PATH) {
        g_state.family = ControllerFamily::Unknown;
        diag::log_line("openxr input: no controller profile bound");
        return;
    }
    char name[XR_MAX_PATH_LENGTH] = {};
    std::uint32_t n = 0;
    xrPathToString(g_instance, ps.interactionProfile, sizeof(name), &n, name);
    g_state.family = std::strstr(name, "htc/vive_controller") ? ControllerFamily::Touchpad
                                                               : ControllerFamily::Thumbstick;
    diag::logf("openxr input: controller profile {}", name);
}

const ControllerState& state() noexcept { return g_state; }

} // namespace mothervr::runtime::openxr::input
