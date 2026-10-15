// The stereo manager and device the game is given in place of its own.
//
// From: FUN_10003510 / FUN_10003640 (init), the StereoInterface vtable
// 0x1005a5c0 and the StereoManager vtable 0x1005a61c.
// Analysis: dumps/06a_stereo_interface.md.

#include "game/stereo/stereo_objects.h"

#include "game/addresses.h"

namespace omvr::game::stereo {

namespace {

// The vtables the game is handed: static storage, they outlive every use.
void* g_device_vtable[22]{};
void* g_manager_vtable[3]{};

} // namespace

void initialize(StereoManager& g_manager, const DeviceSlots& device, const ManagerSlots& manager) noexcept {
    for (std::size_t i = 0; i < device.size(); ++i) g_device_vtable[i] = device[i];
    // The game's own device code for eye transforms and FOV. It reads our
    // device through the game layout.
    g_device_vtable[12] = ai_address(AiAddr::GameDeviceEyeTransform);
    g_device_vtable[13] = ai_address(AiAddr::GameDeviceFovAspect);
    g_device_vtable[14] = ai_address(AiAddr::GameDeviceEyeScaleOffset);
    for (std::size_t i = 0; i < manager.size(); ++i) g_manager_vtable[i] = manager[i];

    // Manager: game defaults (FUN_10003510).
    auto& m = g_manager;
    m.vtable = g_manager_vtable;
    for (int i = 0; i < 4; ++i) m.game_matrix[i * 5] = 1.0f;
    m.game_64 = 1.0f;
    m.game_a8 = 1;
    m.fade    = 1.0f;
    // Reference pose rows (-1,0,0,0), (0,-1,0,0), (0,0,1,0); the game uses identity.
    m.reference_pose[0]  = -1.0f;
    m.reference_pose[5]  = -1.0f;
    m.reference_pose[10] =  1.0f;

    // Device (FUN_10003640).
    auto& si = m.si;
    si.vtable      = g_device_vtable;
    si.world_scale = 1.0f;
}

} // namespace omvr::game::stereo
