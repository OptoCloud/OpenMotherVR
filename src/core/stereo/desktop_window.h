// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "util/sync/once_latch.h"

#include <cstdint>
#include <optional>

namespace omvr::core::stereo {

class StereoDevice;

// The back buffer for a headset session (03 §9.2): with a headset attached
// the game asks for one the size of both eyes (it takes its display mode
// from GetRenderTargetSize), but only the desktop mirror draws into it.
// Give it the window size the game asked for instead: the mirror reads the
// size from the buffer itself, and a stereo-sized buffer cost 0.3 ms GPU and
// 260 MB at 8064x4032. Nullopt to keep `requested`: the window size is not
// known, the request is "the window's size" (zero), or already that size.
// Pure.
std::optional<PixelSize> mirror_back_buffer(PixelSize requested, PixelSize window) noexcept;

// The desktop window while a headset is attached: it is only a mirror.
// Render thread (the swapchain's methods).
class DesktopWindow final {
public:
    DesktopWindow(const StereoDevice& stereo, diag::ports::Log& log) noexcept : stereo_(stereo), log_(log) {}

    // A headset is attached (the game's stereo manager has a device).
    bool headset_attached() const noexcept;

    // With a headset attached the back buffer is both eyes side by side, far
    // past any display mode: refuse to go fullscreen (03 §9.4; the swapchain
    // is created windowed for the same reason). Logs the first refusal.
    bool refuse_fullscreen(bool fullscreen) noexcept;

    // mirror_back_buffer for the attached device's window size; true when
    // `width` x `height` was changed.
    bool mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept;

private:
    const StereoDevice& stereo_;
    diag::ports::Log& log_;
    util::OnceLatch refusal_logged_;
};

} // namespace omvr::core::stereo
