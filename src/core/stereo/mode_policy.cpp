// From: FUN_100201e0, FUN_10020220, FUN_10020c80..FUN_10020dd0 (the mode
// wrappers). Analysis: dumps/06c_detours_and_patches.md.

#include "core/stereo/mode_policy.h"

#include <string_view>
#include <utility>

namespace omvr::core::stereo {

namespace {

constexpr std::string_view kAppendFailedLog = "[stereo] the game's mode-stack append failed; count left at 0";
constexpr std::string_view kNoStorageLog =
    "[stereo] mode 1 entered without a push and the mode stack has no storage; count left at 0";

} // namespace

ModeSteps ModePolicy::push(StereoMode current, StereoMode requested) noexcept {
    if (requested == StereoMode::Screen && current == StereoMode::World) return {.push = StereoMode::Screen};
    return {.pop = true, .push = requested};
}

ModeSteps ModePolicy::pop(StereoMode current) noexcept {
    return {.pop_to_world = current == StereoMode::Screen};
}

ModeSteps ModePolicy::enter_screen_mode(StereoMode current) noexcept {
    return {.pop = current != StereoMode::World, .push = StereoMode::Screen};
}

ModeSteps ModePolicy::leave_screen_mode(StereoMode current) noexcept {
    return {.pop_to_world = current == StereoMode::Screen};
}

// Lowering writes the count. Raising does not, because when the game
// entered mode 1 without a push the array may have no storage (data null,
// capacity 0) and a count of 1 over it makes the game's next pop read
// element 0 through null. It appends 1 through the game's own routine, which
// allocates (on this thread, where the game pushes too). Without that
// routine the count is raised only into storage the game already has, else
// left as it is (the pop to the world still gets back to mode 0).
StackSync ModePolicy::sync_mode_stack(const ModeStackState& stack, bool can_append) noexcept {
    const auto mode = std::to_underlying(stack.mode);
    if (mode >= 0 && stack.count > mode) return StackSync::Lower;
    if (stack.mode != StereoMode::Screen || stack.count != 0) return StackSync::None;
    if (can_append) return StackSync::Append;
    if (stack.has_storage && stack.capacity >= 1) return StackSync::WriteInPlace;
    return StackSync::NoStorage;
}

void ModePolicy::report(StackSyncOutcome outcome) noexcept {
    if (outcome == StackSyncOutcome::AppendFailed && append_failed_logged_.first())
        log_.line(kAppendFailedLog);
    if (outcome == StackSyncOutcome::NoStorage && no_storage_logged_.first()) log_.line(kNoStorageLog);
}

} // namespace omvr::core::stereo
