#pragma once

#include "game/layout.h" // R: stereo (A3): the window size as a value

#include <cstdint>

namespace omvr::core::stereo {

// The desktop window while a headset is attached: it is only a mirror.
class DesktopWindow {
public:
    explicit DesktopWindow(game::StereoManager& manager) noexcept : manager_(manager) {}

    // A headset is attached (the game's stereo manager has a device).
    bool headset_attached() const noexcept;

    // With a headset attached the back buffer is both eyes side by side, far
    // past any display mode: refuse to go fullscreen (the swapchain is
    // created windowed for the same reason). Logs the first refusal.
    bool refuse_fullscreen(bool fullscreen) noexcept;

    // With a headset attached the game asks for a back buffer the size of both
    // eyes (it takes its display mode from GetRenderTargetSize), but only the
    // desktop mirror draws into it. Give it the window size the game asked for
    // instead: the mirror reads the size from the buffer itself, and a stereo-
    // sized buffer cost 0.3 ms GPU and 260 MB at 8064x4032. True when
    // `width` x `height` was changed.
    bool mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept;

private:
    game::StereoManager& manager_;
    bool                 s_logged = false;
};

} // namespace omvr::core::stereo
