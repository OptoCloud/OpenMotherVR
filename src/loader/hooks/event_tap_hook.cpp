// OpenMotherVR-Provenance: clean
#include "loader/hooks/event_tap_hook.h"

#include "loader/context.h"

#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// The script event tap (05 §12): __thiscall(this, const char*), callee pops
// the argument, so __fastcall with the unused edx matches it. Game thread.
void* __fastcall tap(void* self, void* edx, const char* text) noexcept {
    auto& ctx = context();
    if (text && game::in_game_image(text)) {
        const std::string_view event{text};
        const bool changed = ctx.interactions.on_event(event);
        ctx.play.on_event(event);
        if (ctx.settings.current().debug.debug) ctx.interactions.log_event(event, changed);
    }
    return ctx.targets.event_tap(self, edx, text);
}

static_assert(std::is_convertible_v<decltype(&tap), game::TapFn>);

} // namespace

game::TapFn event_tap_replacement() noexcept { return &tap; }

} // namespace omvr::loader::hooks
