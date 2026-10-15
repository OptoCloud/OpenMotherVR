// From: FUN_100201e0, FUN_10020220 (the mode wrappers around these).
// Analysis: dumps/06c_detours_and_patches.md.

#include "game/stereo/mode_stack.h"

#include "game/code_address.h"
#include "game/stereo/stereo_objects.h"

#include <string_view>
#include <utility>

namespace omvr::game::stereo {

namespace {

using core::stereo::ModeStackState;
using core::stereo::ModeSteps;
using core::stereo::StackSync;
using core::stereo::StackSyncOutcome;
using core::stereo::StereoMode;

constexpr std::string_view kModeChangeLog = "[diag] mode {} -> {}";

constexpr std::int32_t kWorld  = std::to_underlying(StereoMode::World);
constexpr std::int32_t kScreen = std::to_underlying(StereoMode::Screen);

void log_mode_change(core::diag::ports::Log& log, std::int32_t before, std::int32_t after) noexcept {
    if (before != after && log.debug_enabled()) core::diag::ports::logf(log, kModeChangeLog, before, after);
}

void push_mode(const AddressMap& addresses, core::diag::ports::Log& log, StereoManager& sm,
               std::int32_t mode) noexcept {
    const std::int32_t before = sm.mode;
    const abi::PushMode push = CodeAddress{addresses.at(AiAddr::GamePushMode)};
    push(&sm, nullptr, mode);
    log_mode_change(log, before, sm.mode);
}

void pop_mode(const AddressMap& addresses, core::diag::ports::Log& log, StereoManager& sm) noexcept {
    const std::int32_t before = sm.mode;
    const abi::PopMode pop = CodeAddress{addresses.at(AiAddr::GamePopMode)};
    pop(&sm, nullptr);
    log_mode_change(log, before, sm.mode);
}

// Leave mode 1 for the world. The game's pop does nothing with an empty
// stack, which is the state when the game entered mode 1 without a push and
// there was no storage to raise the count into (apply_stack_sync): push 0
// through the game instead, which runs its SetMode(0) and appends with its
// own allocator, then drop that entry so the stack is empty in mode 0.
void pop_to_world(const AddressMap& addresses, core::diag::ports::Log& log, StereoManager& sm) noexcept {
    pop_mode(addresses, log, sm);
    if (sm.mode == kScreen && sm.stack_top == 0) {
        push_mode(addresses, log, sm, kWorld);
        sm.stack_top = 0;
    }
}

} // namespace

ModeStackState mode_stack_state(const StereoManager& sm) noexcept {
    return {.mode        = static_cast<StereoMode>(sm.mode),
            .count       = sm.stack_top,
            .capacity    = sm.stack_capacity,
            .has_storage = sm.stack_data != nullptr};
}

void apply_mode_steps(const AddressMap& addresses, core::diag::ports::Log& log, StereoManager& sm,
                      const ModeSteps& steps) noexcept {
    if (steps.pop_to_world) {
        pop_to_world(addresses, log, sm);
    } else if (steps.pop) {
        pop_mode(addresses, log, sm);
    }
    if (steps.push) push_mode(addresses, log, sm, std::to_underlying(*steps.push));
}

StackSyncOutcome apply_stack_sync(const ModeStackTargets& targets, StereoManager& sm, StackSync sync) noexcept {
    switch (sync) {
    case StackSync::None:
        break;
    case StackSync::Lower:
        sm.stack_top = sm.mode;
        break;
    case StackSync::Append: {
        const std::int32_t      screen = kScreen;
        const ModeStackAppendFn append = CodeAddress{targets.append};
        if (!append(&sm.stack_data, nullptr, &screen)) {
            // The routine raises the count even when its allocation failed,
            // over a null array: take it back.
            sm.stack_top = 0;
            return StackSyncOutcome::AppendFailed;
        }
        break;
    }
    case StackSync::WriteInPlace:
        sm.stack_data[0] = kScreen;
        sm.stack_top     = 1;  // one entry
        break;
    case StackSync::NoStorage:
        return StackSyncOutcome::NoStorage;
    }
    return StackSyncOutcome::Done;
}

} // namespace omvr::game::stereo
