#pragma once

namespace omvr::loader::hooks {

// Pushes the settings into the menu `ui` that just opened. Menus open
// (front end, pause, loading screen) after the game has set up whatever
// exception handling of its own it installs, and now and then during play:
// a cheap moment, on the game thread, to make sure a crash still reaches
// the crash record, so the crash filter is put back on top first.
void push_settings(void* ui) noexcept;

// Installed over the menu vtable slot at DAT_10066d9c. Runs the stock method,
// then pushes the settings into the menu that just opened.
//
// From: FUN_10020c60 @ 0x10020c60.
void __fastcall on_menu_open(void* self, void* edx) noexcept;

// Console command handler installed into AI.exe's command dispatch. Parses
// and handles "mvr_*" commands, updating the settings in place. Non-mvr_
// tokens are forwarded to the original handler.
//
// Installed over the vtable slot at DAT_10066dc0, so AI.exe calls it
// __thiscall like the handler it replaces (the original ends in `ret 0x10`).
//
// From: FUN_100202e0 @ 0x100202e0 (size 1418 bytes).
void __fastcall handle_mvr_command(void* self, void* edx, int ctx, char* cmd, int p3, int p4) noexcept;

// Runtime None: the pause menu's open runs the game's method and pushes the
// settings, so the options page reached from the pause menu shows them. No
// stereo mode change: there is no stereo device.
// From: FUN_10020ca0, without its stereo mode push.
void __fastcall pause_open_settings_only(void* self, void* edx) noexcept;

} // namespace omvr::loader::hooks
