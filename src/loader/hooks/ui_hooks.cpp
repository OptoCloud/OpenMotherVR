#include "loader/hooks/ui_hooks.h"

#include "core/ui/commands.h"
#include "game/ui/console.h"
#include "loader/context.h"
#include "loader/hooks/crash_hook.h"
#include "util/sync/once_latch.h"

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

void push_settings(void* ui) noexcept {
    reassert_crash_hook();
    context().settings_push.push(core::ui::ports::UiHandle{reinterpret_cast<std::uintptr_t>(ui)});
}

void __fastcall on_menu_open(void* self, void* /*edx*/) noexcept {
    // Once: the menu opens many times a session. Trivially destructible, so
    // nothing runs at exit.
    static util::OnceLatch logged;
    auto& ctx = context();
    if (logged.first())
        core::diag::ports::logf(ctx.log, "[script_bridge] menu opened (ui={}), pushing settings", self);
    ctx.console.open_menu(self);
    push_settings(self);
}

// Note: `cmd` is the raw console input with the first whitespace-terminated
// token as the command name. Only mvr_ commands are tokenised (in place);
// the original also tokenised the others before forwarding them, which cut
// their arguments off.
void __fastcall handle_mvr_command(void* self, void* /*edx*/, int ctx, char* cmd, int p3, int p4) noexcept {
    // A null name is not ours to judge: the stock method gets the call
    // unchanged, as every call that is not an mvr_ command does.
    if (!cmd || !core::ui::is_mod_command(std::string_view{cmd})) {
        context().console.forward_command(self, ctx, cmd, p3, p4);
        return;
    }
    context().commands.run(cmd);
}

void __fastcall pause_open_settings_only(void* self, void* /*edx*/) noexcept {
    // The stock method this replaces, as the installer recorded it.
    if (auto open = reinterpret_cast<game::ui::MenuMethodFn>(context().targets.pause_open_settings_only))
        open(self, nullptr);
    push_settings(self);
}

static_assert(std::is_convertible_v<decltype(&on_menu_open), game::ui::MenuMethodFn>);
static_assert(std::is_convertible_v<decltype(&pause_open_settings_only), game::ui::MenuMethodFn>);
static_assert(std::is_convertible_v<decltype(&handle_mvr_command), game::ui::CmdHandlerFn>);

} // namespace omvr::loader::hooks
