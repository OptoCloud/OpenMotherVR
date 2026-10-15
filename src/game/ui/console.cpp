#include "game/ui/console.h"

namespace omvr::game::ui {

void Console::bind(PFN_CmdHandler original_cmd, PFN_MenuMethod original_menu_open, std::uint8_t* exit_request,
                   std::uint8_t* display_reset_flag) noexcept {
    g_original_cmd       = original_cmd;
    g_original_menu_open = original_menu_open;
    g_exit_request       = exit_request;
    g_display_reset_flag = display_reset_flag;
}

void Console::forward_command(void* self, int ctx, char* cmd, int p3, int p4) const noexcept {
    if (g_original_cmd) g_original_cmd(self, nullptr, ctx, cmd, p3, p4);
}

void Console::open_menu(void* self) const noexcept {
    if (g_original_menu_open) g_original_menu_open(self, nullptr);
}

void Console::request_exit() noexcept {
    if (g_exit_request) *g_exit_request = 1;
}

void Console::request_display_reset() noexcept {
    if (g_display_reset_flag) *g_display_reset_flag = 1;
}

bool Console::can_reset_display() const noexcept { return g_display_reset_flag != nullptr; }

} // namespace omvr::game::ui
