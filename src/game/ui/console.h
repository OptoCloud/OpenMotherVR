#pragma once

#include "core/ui/ports/game_signals.h"

#include <cstdint>

namespace omvr::game::ui {

// AI.exe's original console-command handler, chained for non-mvr_ tokens.
// From: DAT_10066c20.
using PFN_CmdHandler = void (__fastcall*)(void* self, void* edx, int ctx, char* cmd, int p3, int p4);

// A no-argument menu method, the stock implementation behind a hooked slot.
// From: DAT_10066c10.
using PFN_MenuMethod = void (__fastcall*)(void* self, void* edx);

// The game's console command handler, its front-end menu's open method and
// two request bytes its frame loop reads.
class Console final : public core::ui::ports::GameSignals {
public:
    // AI.exe's exit-request byte: a plain global with a getter and setter
    // (2015: 0xa14760 / 0xa14770) read by the frame loop; setting it quits the
    // game. The original sets it after the menu changes the VR runtime
    // (mvr_vrruntime) and when the Oculus runtime asks the app to quit.
    // From: DAT_10066d58.
    //
    // AI.exe's "display changed" byte: when set, the game's frame loop rebuilds
    // its render targets (as after a window resize). Raised when a settings
    // push or console command changes the mirror or pixel scale, so those take
    // effect without a restart.
    // From: DAT_10066d44 (AI.exe table slot 0x5F).
    void bind(PFN_CmdHandler original_cmd, PFN_MenuMethod original_menu_open, std::uint8_t* exit_request,
              std::uint8_t* display_reset_flag) noexcept;

    // The game's own handler, for every command that is not an mvr_ one.
    void forward_command(void* self, int ctx, char* cmd, int p3, int p4) const noexcept;
    // The front-end menu's stock open method.
    void open_menu(void* self) const noexcept;

    void request_exit() noexcept override;
    void request_display_reset() noexcept override;
    bool can_reset_display() const noexcept override;

private:
    PFN_CmdHandler g_original_cmd       = nullptr;
    PFN_MenuMethod g_original_menu_open = nullptr;
    std::uint8_t*  g_exit_request       = nullptr;
    std::uint8_t*  g_display_reset_flag = nullptr;
};

} // namespace omvr::game::ui
