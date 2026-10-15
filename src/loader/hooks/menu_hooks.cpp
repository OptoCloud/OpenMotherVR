#include "loader/hooks/menu_hooks.h"

#include "core/stereo/mode_policy.h"
#include "game/code_address.h"
#include "game/stereo/mode_stack.h"
#include "game/stereo/scene_messages.h"
#include "loader/context.h"
#include "loader/hooks/ui_hooks.h"

#include <type_traits>

namespace omvr::loader::hooks {

namespace {

using core::stereo::ModePolicy;
using core::stereo::StereoMode;

// The game's methods these replace (05 §5, §6): thiscall, as
// __fastcall(self, edx, ...).
using FollowFn  = int(__fastcall*)(void* self, void* edx, void* arg); // ret 4
using MenuFn    = void(__fastcall*)(void* self, void* edx);           // ret 0
using MessageFn = int(__fastcall*)(void* self, void* edx, const std::uint32_t* msg, int a, int b,
                                   int c); // ret 0x10

static_assert(std::is_convertible_v<decltype(&follow_on_activate), FollowFn>);
static_assert(std::is_convertible_v<decltype(&follow_on_deactivate), FollowFn>);
static_assert(std::is_convertible_v<decltype(&ui_on_message), MessageFn>);
static_assert(std::is_convertible_v<decltype(&menu_close), MenuFn>);
static_assert(std::is_convertible_v<decltype(&pause_open), MenuFn>);
static_assert(std::is_convertible_v<decltype(&pause_close), MenuFn>);
static_assert(std::is_convertible_v<decltype(&loading_open), MenuFn>);
static_assert(std::is_convertible_v<decltype(&loading_close), MenuFn>);
static_assert(std::is_convertible_v<decltype(&attract_open), MenuFn>);
static_assert(std::is_convertible_v<decltype(&attract_close), MenuFn>);

// The game's own method `fn`, when the installer found one.
void call_menu(void* fn, void* self) noexcept {
    if (!fn) return;
    const MenuFn menu = game::CodeAddress{fn};
    menu(self, nullptr);
}

StereoMode current_mode(Context& ctx) noexcept { return static_cast<StereoMode>(ctx.manager.mode); }

// Show the virtual screen while a menu is up (05 §5).
void enter_screen_mode() noexcept {
    auto& ctx = context();
    game::stereo::apply_mode_steps(ctx.addresses, ctx.log, ctx.manager,
                                   ModePolicy::enter_screen_mode(current_mode(ctx)));
}

void leave_screen_mode() noexcept {
    auto& ctx = context();
    game::stereo::apply_mode_steps(ctx.addresses, ctx.log, ctx.manager,
                                   ModePolicy::leave_screen_mode(current_mode(ctx)));
}

} // namespace

// From: FUN_10001720 / FUN_10001740.
int __fastcall follow_on_activate(void* self, void*, void* arg) noexcept {
    auto& ctx = context();
    ctx.follow.on_activate(self);
    const FollowFn activate = game::CodeAddress{ctx.targets.game_side.follow_on_activate};
    return activate(self, nullptr, arg);
}

// Unlike the original, forget the camera once it is deactivated: the
// stereo update writes through it every frame, and the object may be freed
// after this (05 §6.1).
int __fastcall follow_on_deactivate(void* self, void*, void* arg) noexcept {
    auto& ctx = context();
    ctx.follow.on_deactivate_begin();
    const FollowFn deactivate = game::CodeAddress{ctx.targets.game_side.follow_on_deactivate};
    const int r               = deactivate(self, nullptr, arg);
    ctx.follow.on_deactivate_end(self);
    return r;
}

// Watch for cutscene and level-fade start/finish messages (05 §6.2).
// From: FUN_1001f830 (naked in the original, reading the caller's frame;
// we read our own first argument).
int __fastcall ui_on_message(void* self, void*, const std::uint32_t* msg, int a, int b, int c) noexcept {
    auto& ctx              = context();
    const std::uint32_t id = msg ? *msg : 0;
    ctx.scene.on_message(id, game::stereo::scene_messages(ctx.addresses));
    const MessageFn on_message = game::CodeAddress{ctx.targets.game_side.ui_on_message};
    return on_message(self, nullptr, msg, a, b, c);
}

// From: FUN_10020c80.
void __fastcall menu_close(void* self, void*) noexcept {
    call_menu(context().targets.game_side.menu_close, self);
    context().play.set_screen(core::ui::Screen::None);
    leave_screen_mode();
}

// From: FUN_10020ca0.
void __fastcall pause_open(void* self, void*) noexcept {
    call_menu(context().targets.game_side.pause_open, self);
    context().play.set_screen(core::ui::Screen::Pause);
    enter_screen_mode();
    push_settings(self);
}

// From: FUN_10020cf0.
void __fastcall pause_close(void* self, void*) noexcept {
    call_menu(context().targets.game_side.pause_close, self);
    context().play.set_screen(core::ui::Screen::None);
    leave_screen_mode();
}

// From: FUN_10020d10. The 3D scene goes black behind the loading screen.
void __fastcall loading_open(void* self, void*) noexcept {
    call_menu(context().targets.game_side.loading_open, self);
    context().play.set_screen(core::ui::Screen::Loading);
    enter_screen_mode();
    context().update.set_loading_blackout(true);
    push_settings(self);
}

// From: FUN_10020d60.
void __fastcall loading_close(void* self, void*) noexcept {
    call_menu(context().targets.game_side.loading_close, self);
    context().play.set_screen(core::ui::Screen::None);
    context().update.set_loading_blackout(false);
    leave_screen_mode();
    // Entering gameplay: forward is where the player faces now.
    context().update.request_recenter();
}

// From: FUN_10020da0.
void __fastcall attract_open(void* self, void*) noexcept {
    call_menu(context().targets.game_side.attract_open, self);
    context().play.set_screen(core::ui::Screen::Title);
    enter_screen_mode();
}

// From: FUN_10020dd0 (the game's close is an empty method).
void __fastcall attract_close(void*, void*) noexcept {
    context().play.set_screen(core::ui::Screen::None);
    leave_screen_mode();
}

} // namespace omvr::loader::hooks
