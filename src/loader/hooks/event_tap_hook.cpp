#include "loader/hooks/event_tap_hook.h"

#include "core/config/settings_store.h"
#include "loader/context.h"

#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// __thiscall(this, const char*), callee pops the argument: __fastcall with
// the unused edx matches it.
void* __fastcall tap(void* self, void* edx, const char* text) noexcept {
    auto& ctx = context();
    if (text && game::in_game_image(text)) {
        const std::string_view s{text};
        const bool changed = ctx.interactions.on_event(s);
        if (core::config::current().debug.debug) ctx.interactions.log_event(s, changed);
    }
    return ctx.originals.event_tap(self, edx, text);
}

static_assert(std::is_convertible_v<decltype(&tap), game::TapFn>);

} // namespace

game::TapFn event_tap_replacement() noexcept { return &tap; }

} // namespace omvr::loader::hooks
