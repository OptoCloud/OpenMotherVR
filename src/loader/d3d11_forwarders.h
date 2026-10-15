#pragma once

#include "platform/d3d11/system_d3d11.h"

#include <cstdint>

namespace omvr::loader {

// Load System32\d3d11.dll (SysWOW64 for this 32-bit process) and resolve
// D3D11CreateDeviceAndSwapChain, the one function AI.exe imports, which we
// intercept, plus the 50 exports we pass through verbatim for other modules
// (d3d11_exports.inc).
//
// Runs once, on the first call of any of our exports, not in DllMain:
// LoadLibrary under the loader lock is a deadlock hazard. Thread-safe;
// later calls return the first result at once.
bool ensure_d3d11_loaded() noexcept;

// What the exported D3D11CreateDeviceAndSwapChain needs of the context. Its
// file includes no D3D11 header (it would redeclare the export), so these
// stand between it and loader/context.h.
namespace entry {
platform::d3d11::PFN_D3D11CreateDeviceAndSwapChain real_create_device_and_swapchain() noexcept;
// The attach-time message boxes are up until the player closes them.
void wait_for_dialogs() noexcept;
// Runtime None: the unmodded game.
bool ui_set_only() noexcept;
bool headset_attached() noexcept;
bool mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept;
} // namespace entry

} // namespace omvr::loader
