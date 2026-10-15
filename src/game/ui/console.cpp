#include "game/ui/console.h"

namespace omvr::game::ui {

void Console::bind(CmdHandlerFn original_cmd, MenuMethodFn original_menu_open, std::uint8_t* exit_request,
                   std::uint8_t* display_reset_flag) noexcept {
    original_cmd_       = original_cmd;
    original_menu_open_ = original_menu_open;
    exit_request_       = exit_request;
    display_reset_flag_ = display_reset_flag;
}

void Console::forward_command(void* self, int ctx, char* cmd, int p3, int p4) const noexcept {
    if (original_cmd_) original_cmd_(self, nullptr, ctx, cmd, p3, p4);
}

void Console::open_menu(void* self) const noexcept {
    if (original_menu_open_) original_menu_open_(self, nullptr);
}

void Console::request_exit() noexcept {
    if (exit_request_) *exit_request_ = kRequestRaised;
}

void Console::request_display_reset() noexcept {
    if (display_reset_flag_) *display_reset_flag_ = kRequestRaised;
}

bool Console::can_reset_display() const noexcept { return display_reset_flag_ != nullptr; }

} // namespace omvr::game::ui
