#include "loader/hooks/ui_hooks.h"

#include "core/diag/logger.h"
#include "game/ui/console.h"
#include "loader/context.h"
#include "loader/hooks/crash_hook.h"

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace omvr::loader::hooks {

namespace diag = core::diag;

void push_settings(void* ui) noexcept {
    reassert_crash_hook();
    context().settings_push.push(core::ui::ports::UiHandle{reinterpret_cast<std::uintptr_t>(ui)});
}

void __fastcall on_menu_open(void* self, void* /*edx*/) noexcept {
    static bool s_logged = false;
    if (!std::exchange(s_logged, true)) {
        diag::logf("[script_bridge] menu opened (ui={}), pushing settings", self);
    }
    context().console.open_menu(self);
    push_settings(self);
}

// Note: `cmd` is the raw console input with the first whitespace-terminated
// token as the command name. Only mvr_ commands are tokenised (in place);
// the original also tokenised the others before forwarding them, which cut
// their arguments off.
void __fastcall handle_mvr_command(void* self, void* /*edx*/, int ctx, char* cmd, int p3, int p4) noexcept {
    // A null name is not ours to judge: the stock method gets the call
    // unchanged, as every call that is not an mvr_ command does.
    if (!cmd) {
        context().console.forward_command(self, ctx, cmd, p3, p4);
        return;
    }

    // Non-mvr_ commands go to the original handler untouched.
    const char* first = cmd;
    while (*first == ' ') ++first;
    if (std::strncmp(first, "mvr_", 4) != 0) {
        context().console.forward_command(self, ctx, cmd, p3, p4);
        return;
    }
    context().commands.run(cmd);
}

void __fastcall pause_open_settings_only(void* self, void* /*edx*/) noexcept {
    using game::ui::PFN_MenuMethod;
    if (auto open = reinterpret_cast<PFN_MenuMethod>(context().originals.pause_open_settings_only)) open(self, nullptr);
    push_settings(self);
}

static_assert(std::is_convertible_v<decltype(&on_menu_open), game::ui::PFN_MenuMethod>);
static_assert(std::is_convertible_v<decltype(&pause_open_settings_only), game::ui::PFN_MenuMethod>);
static_assert(std::is_convertible_v<decltype(&handle_mvr_command), game::ui::PFN_CmdHandler>);

} // namespace omvr::loader::hooks
