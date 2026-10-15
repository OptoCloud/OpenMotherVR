#pragma once

#include "game/layout.h"
#include "game/stereo/stereo_objects.h"

namespace omvr::loader::hooks {

// The stereo device's and manager's vtables (see game/stereo/stereo_objects.h);
// the device's slots 12 to 14 are left for the game's own functions.
game::stereo::DeviceSlots device_slots() noexcept;
game::stereo::ManagerSlots manager_slots() noexcept;

// Replacements for three game stereo-manager calls (all __thiscall on the
// manager, installed as call-site redirects by the hook installer).
//
// Manager::CreateDevice(int enable): bring up the VR runtime and attach
// the device. Only the first call starts the runtime; later ones (a display
// reset) re-attach what it produced. From: FUN_1001f6a0 (ret 4).
void __fastcall create_device(game::StereoManager* self, void* edx, int enable) noexcept;
// Manager::GetRenderTargetSize(int* w, int* h): record the desktop window
// size, then size the stereo target. From: FUN_100202a0 (ret 8).
int* __fastcall get_render_target_size(game::StereoManager* self, void* edx, int* w, int* h) noexcept;
// Manager::GetHmdResolution(int* w, int* h): leave the window size alone.
// From: FUN_1001f640 (ret 8).
int* __fastcall get_hmd_resolution(game::StereoManager* self, void* edx, int* w, int* h) noexcept;

} // namespace omvr::loader::hooks
