#pragma once

#include <cstdint>

// The vtable replacements around the menus: the first-person camera
// tracker, the cutscene and fade watcher, and the menu wrappers that switch
// to the virtual screen. From: FUN_10020c80..FUN_10020dd0, FUN_10001720,
// FUN_10001740, FUN_1001f830. Analysis: dumps/06c_detours_and_patches.md.
namespace omvr::loader::hooks {

// FollowBehavior OnActivate / OnDeactivate(arg). __thiscall, ret 4.
int __fastcall follow_on_activate(void* self, void* edx, void* arg) noexcept;
int __fastcall follow_on_deactivate(void* self, void* edx, void* arg) noexcept;

// UI entity OnMessage(const ShortGuid*, a, b, c). __thiscall, ret 0x10.
int __fastcall ui_on_message(void* self, void* edx, const std::uint32_t* msg, int a, int b, int c) noexcept;

// Menu Open / Close. __thiscall, no args.
void __fastcall menu_close(void* self, void* edx) noexcept;
void __fastcall pause_open(void* self, void* edx) noexcept;
void __fastcall pause_close(void* self, void* edx) noexcept;
void __fastcall loading_open(void* self, void* edx) noexcept;
void __fastcall loading_close(void* self, void* edx) noexcept;
void __fastcall attract_open(void* self, void* edx) noexcept;
void __fastcall attract_close(void* self, void* edx) noexcept;

} // namespace omvr::loader::hooks
