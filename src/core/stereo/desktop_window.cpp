#include "core/stereo/desktop_window.h"

#include "core/diag/logger.h"
#include "game/stereo/stereo_objects.h" // R: stereo (A3): the window size as a value

#include <utility>

namespace omvr::core::stereo {

bool DesktopWindow::headset_attached() const noexcept { return manager_.device != nullptr; }

bool DesktopWindow::refuse_fullscreen(bool fullscreen) noexcept {
    if (!fullscreen || !headset_attached()) return false;
    if (!std::exchange(s_logged, true)) {
        diag::log_line("proxy: VR active, fullscreen refused");
    }
    return true;
}

bool DesktopWindow::mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept {
    using UINT = std::uint32_t;
    const auto* si = game::stereo::load_device(manager_);
    if (!si || si->mirror_width <= 0 || si->mirror_height <= 0) return false;
    if (width == 0 || height == 0) return false; // "the window's size": already small
    if (width == static_cast<UINT>(si->mirror_width) && height == static_cast<UINT>(si->mirror_height))
        return false;
    width  = static_cast<UINT>(si->mirror_width);
    height = static_cast<UINT>(si->mirror_height);
    return true;
}

} // namespace omvr::core::stereo
