#include "loader/hooks/menu_hooks.h"

#include "loader/context.h"
#include "loader/hooks/ui_hooks.h"

namespace omvr::loader::hooks {

namespace {

using FollowFn = int (__fastcall*)(void*, void*, void*);
using MenuFn   = void (__fastcall*)(void*, void*);

void call_menu(void* fn, void* self) noexcept {
    if (fn) reinterpret_cast<MenuFn>(fn)(self, nullptr);
}

// Show the virtual screen while a menu is up.
void enter_screen_mode() noexcept { context().modes.enter_screen_mode(); }

void leave_screen_mode() noexcept { context().modes.leave_screen_mode(); }

} // namespace

// From: FUN_10001720 / FUN_10001740.
int __fastcall follow_on_activate(void* self, void*, void* arg) noexcept {
    context().follow.g_follow = static_cast<std::uint8_t*>(self);
    context().follow.g_follow_active = true;
    return reinterpret_cast<FollowFn>(context().originals.game_side.follow_on_activate)(self, nullptr, arg);
}

// Unlike the original, forget the camera once it is deactivated: the
// stereo update writes through g_follow every frame, and the object may be
// freed after this.
int __fastcall follow_on_deactivate(void* self, void*, void* arg) noexcept {
    context().follow.g_follow_active = false;
    const int r = reinterpret_cast<FollowFn>(context().originals.game_side.follow_on_deactivate)(self, nullptr, arg);
    if (context().follow.g_follow == self) context().follow.g_follow = nullptr;
    return r;
}

// Watch for cutscene and level-fade start/finish messages.
// From: FUN_1001f830 (naked in the original, reading the caller's frame;
// we read our own first argument).
int __fastcall ui_on_message(void* self, void*, const std::uint32_t* msg, int a, int b, int c) noexcept {
    const std::uint32_t id = msg ? *msg : 0;
    context().scene.on_message(id);
    using Fn = int (__fastcall*)(void*, void*, const std::uint32_t*, int, int, int);
    return reinterpret_cast<Fn>(context().originals.game_side.ui_on_message)(self, nullptr, msg, a, b, c);
}

// From: FUN_10020c80.
void __fastcall menu_close(void* self, void*) noexcept {
    call_menu(context().originals.game_side.menu_close, self);
    leave_screen_mode();
}

// From: FUN_10020ca0.
void __fastcall pause_open(void* self, void*) noexcept {
    call_menu(context().originals.game_side.pause_open, self);
    enter_screen_mode();
    push_settings(self);
}

// From: FUN_10020cf0.
void __fastcall pause_close(void* self, void*) noexcept {
    call_menu(context().originals.game_side.pause_close, self);
    leave_screen_mode();
}

// From: FUN_10020d10. The 3D scene goes black behind the loading screen.
void __fastcall loading_open(void* self, void*) noexcept {
    call_menu(context().originals.game_side.loading_open, self);
    enter_screen_mode();
    context().update.set_loading_blackout(true);
    push_settings(self);
}

// From: FUN_10020d60.
void __fastcall loading_close(void* self, void*) noexcept {
    call_menu(context().originals.game_side.loading_close, self);
    context().update.set_loading_blackout(false);
    leave_screen_mode();
    // Entering gameplay: forward is where the player faces now.
    context().update.request_recenter();
}

// From: FUN_10020da0.
void __fastcall attract_open(void* self, void*) noexcept {
    call_menu(context().originals.game_side.attract_open, self);
    enter_screen_mode();
}

// From: FUN_10020dd0 (the game's close is an empty method).
void __fastcall attract_close(void*, void*) noexcept {
    leave_screen_mode();
}

} // namespace omvr::loader::hooks
