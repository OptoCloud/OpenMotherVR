#pragma once

#include <cstdint>

namespace mothervr::integration {

// Signatures of the AI.exe entry points MotherVR calls into. The game's UI
// layer uses the Fire* methods to set script variables by name. All are
// resolved by the hook installer from the per-build AI.exe address table
// (DAT_10066bf0..bfc and DAT_10066c20 in the original).
//
// They are all __thiscall methods: `this` in ECX, arguments on the stack,
// callee pops them. A plain function pointer can't be __thiscall, so they are
// declared __fastcall with an unused EDX parameter, which has the same ABI.
// The original does the same (`xor edx, edx` before every call).

// Registers a view-control item (menu row) with two display strings.
// From: DAT_10066bf0. Used for resolution-picker rows in the VR options menu.
using PFN_FireStr2   = void (__fastcall*)(void* ui, void* edx, const char* name,
                                          const char* label, const char* value);

// Sets a boolean script variable by name.
// From: DAT_10066bf4.
using PFN_FireBool   = void (__fastcall*)(void* ui, void* edx, const char* name, int bool_val);

// Sets an integer script variable by name.
// From: DAT_10066bf8.
using PFN_FireInt    = void (__fastcall*)(void* ui, void* edx, const char* name, int value);

// Sets a float script variable by name.
// From: DAT_10066bfc.
using PFN_FireFloat  = void (__fastcall*)(void* ui, void* edx, const char* name, float value);

// AI.exe's original console-command handler, chained for non-mvr_ tokens.
// From: DAT_10066c20.
using PFN_CmdHandler = void (__fastcall*)(void* self, void* edx, int ctx, char* cmd, int p3, int p4);

// A no-argument menu method, the stock implementation behind a hooked slot.
// From: DAT_10066c10.
using PFN_MenuMethod = void (__fastcall*)(void* self, void* edx);

// ---- Globals populated by the hook installer --------------------------------
// All null before hook_installer::install() runs.

extern PFN_FireStr2   g_fire_str2;
extern PFN_FireBool   g_fire_bool;
extern PFN_FireInt    g_fire_int;
extern PFN_FireFloat  g_fire_float;
extern PFN_CmdHandler g_original_cmd;
extern PFN_MenuMethod g_original_menu_open;

// AI.exe's exit-request byte: a plain global with a getter and setter
// (2015: 0xa14760 / 0xa14770) read by the frame loop; setting it quits the
// game. The original sets it after the menu changes the VR runtime
// (mvr_vrruntime) and when the Oculus runtime asks the app to quit.
// From: DAT_10066d58.
extern std::uint8_t*  g_exit_request;

// Set to true once the VR session object has been successfully initialised
// (i.e. we own a live IVRSystem / ovrSession). Read by push_settings_to_game
// to populate the "setMotherVROptionsEnabled" game variable.
//
// Corresponds to the *(StereoManager + 4) != nullptr check in FUN_10020870.
extern bool g_vr_session_active;

// AI.exe's "display changed" byte: when set, the game's frame loop rebuilds
// its render targets (as after a window resize). Raised when a settings push
// or console command changes the mirror or pixel scale, so those take effect
// without a restart; an unchanged push leaves it alone (a rebuild is a
// visible freeze).
// From: DAT_10066d44 (AI.exe table slot 0x5F).
extern std::uint8_t*  g_display_reset_flag;

// ---- Entry points -----------------------------------------------------------

// Push the current config values to the game's UI script-variable layer.
// `ui` is the game's menu object the Fire* methods run on; the original only
// ever calls this from its menu hooks, with their `this`.
//
// From: FUN_10020870 @ 0x10020870 (size 996 bytes).
void push_settings_to_game(void* ui) noexcept;

// Installed over the menu vtable slot at DAT_10066d9c. Runs the stock method,
// then pushes the settings into the menu that just opened.
//
// From: FUN_10020c60 @ 0x10020c60.
void __fastcall on_menu_open(void* self, void* edx) noexcept;

// Console command handler installed into AI.exe's command dispatch. Parses
// and handles "mvr_*" commands, updating config::current() in place.
// Non-mvr_ tokens are forwarded to the original handler via g_original_cmd.
//
// Installed over the vtable slot at DAT_10066dc0, so AI.exe calls it
// __thiscall like the handler it replaces (the original ends in `ret 0x10`).
//
// From: FUN_100202e0 @ 0x100202e0 (size 1418 bytes).
void __fastcall handle_mvr_command(void* self, void* edx, int ctx, char* cmd, int p3, int p4) noexcept;

} // namespace mothervr::integration
