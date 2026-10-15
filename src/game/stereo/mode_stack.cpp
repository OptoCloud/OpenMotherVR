// From: FUN_100201e0, FUN_10020220 (the mode wrappers around these).
// Analysis: dumps/06c_detours_and_patches.md.

#include "game/stereo/mode_stack.h"

#include "core/config/settings_store.h" // R: stereo (A3): the debug flag through the Log port
#include "core/diag/logger.h"           // R: stereo (A3): log through an injected Log&
#include "game/addresses.h"

namespace omvr::game::stereo {

namespace {

namespace config = core::config;
namespace diag = core::diag;

// R: stereo (A3): a member of the object that owns the mode stack.
ModeStackAppendFn g_mode_stack_append = nullptr;

template <class Fn>
Fn game_fn(AiAddr slot) noexcept { return reinterpret_cast<Fn>(ai_address(slot)); }

// Debug: each change of the mode (0 world, 1 virtual screen), to follow
// which menus, videos and loading screens get the screen.
void log_mode_change(int before, int after) noexcept {
    if (before != after && config::current().debug.debug) diag::logf("[diag] mode {} -> {}", before, after);
}

} // namespace

void game_push_mode(StereoManager* sm, int mode) noexcept {
    const int before = sm->mode;
    game_fn<void (__fastcall*)(StereoManager*, void*, int)>(AiAddr::GamePushMode)(sm, nullptr, mode);
    log_mode_change(before, sm->mode);
}
void game_pop_mode(StereoManager* sm) noexcept {
    const int before = sm->mode;
    game_fn<void (__fastcall*)(StereoManager*, void*)>(AiAddr::GamePopMode)(sm, nullptr);
    log_mode_change(before, sm->mode);
}

// Leave mode 1 for the world. The game's pop does nothing with an empty
// stack, which is the state when the game entered mode 1 without a push and
// there was no storage to raise the count into (sync_mode_stack): push 0
// through the game instead, which runs its SetMode(0) and appends with its
// own allocator, then drop that entry so the stack is empty in mode 0.
void game_pop_to_world(StereoManager* sm) noexcept {
    game_pop_mode(sm);
    if (sm->mode == 1 && sm->stack_top == 0) {
        game_push_mode(sm, 0);
        sm->stack_top = 0;
    }
}

void install_mode_stack_append(void* append) noexcept {
    g_mode_stack_append = reinterpret_cast<ModeStackAppendFn>(append);
}

ModeStackAppendFn mode_stack_append() noexcept { return g_mode_stack_append; }

} // namespace omvr::game::stereo
