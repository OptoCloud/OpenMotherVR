// From: FUN_100201e0, FUN_10020220, FUN_10020c80..FUN_10020dd0 (the mode
// wrappers). Analysis: dumps/06c_detours_and_patches.md.

#include "core/stereo/mode_policy.h"

#include "core/diag/logger.h"
#include "game/stereo/mode_stack.h" // R: stereo (A3): the game's stack calls stay in the thunks

#include <utility>

namespace omvr::core::stereo {

using game::StereoManager;
using game::stereo::game_pop_mode;
using game::stereo::game_pop_to_world;
using game::stereo::game_push_mode;

void ModePolicy::push(StereoManager* self, int mode) noexcept {
    if (mode == 1 && self->mode == 0) {
        game_push_mode(self, 1);
        return;
    }
    game_pop_mode(self);
    game_push_mode(self, mode);
}

void ModePolicy::pop(StereoManager* self) noexcept {
    if (self->mode == 1) game_pop_to_world(self);
}

void ModePolicy::enter_screen_mode() noexcept {
    StereoManager& m = manager_;
    if (m.mode != 0) game_pop_mode(&m);
    game_push_mode(&m, 1);
}

void ModePolicy::leave_screen_mode() noexcept {
    StereoManager& m = manager_;
    if (m.mode == 1) game_pop_to_world(&m);
}

// Rule f: at the end of each update the stack count equals the mode, after
// unbalanced game pushes and pops. Lowering writes the count. Raising does
// not, because when the game entered mode 1 without a push the array may
// have no storage (data null, capacity 0) and a count of 1 over it makes
// the game's next pop read element 0 through null. It appends 1 through the
// game's own routine, which allocates (on this thread, where the game
// pushes too). Without that routine the count is raised only into storage
// the game already has, else left as it is (game_pop_to_world still gets
// back to mode 0) and that is logged once.
void ModePolicy::sync_mode_stack(StereoManager& m) noexcept {
    const auto g_mode_stack_append = game::stereo::mode_stack_append();
    if (m.mode >= 0 && m.stack_top > m.mode) {
        m.stack_top = m.mode;
    } else if (m.mode == 1 && m.stack_top == 0) {
        if (g_mode_stack_append) {
            constexpr std::int32_t kScreenMode = 1;
            if (!g_mode_stack_append(&m.stack_data, nullptr, &kScreenMode)) {
                // The routine raises the count even when its allocation
                // failed, over a null array: take it back.
                m.stack_top = 0;
                static bool s_logged = false;
                if (!std::exchange(s_logged, true)) {
                    diag::log_line("[stereo] the game's mode-stack append failed; count left at 0");
                }
            }
        } else if (m.stack_data && m.stack_capacity >= 1) {
            m.stack_data[0] = 1;
            m.stack_top = 1;
        } else {
            static bool s_logged = false;
            if (!std::exchange(s_logged, true)) {
                diag::log_line("[stereo] mode 1 entered without a push and the mode stack has no storage; "
                               "count left at 0");
            }
        }
    }
}

} // namespace omvr::core::stereo
