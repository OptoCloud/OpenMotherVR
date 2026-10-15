#include "core/stereo/desktop_window.h"

#include "core/stereo/stereo_device.h"

#include <string_view>

namespace omvr::core::stereo {

namespace {

constexpr std::string_view kFullscreenRefusedLog = "proxy: VR active, fullscreen refused";

} // namespace

std::optional<PixelSize> mirror_back_buffer(PixelSize requested, PixelSize window) noexcept {
    if (window.width <= 0 || window.height <= 0) return std::nullopt;
    if (requested.width == 0 || requested.height == 0)
        return std::nullopt; // "the window's size": already small
    if (requested == window) return std::nullopt;
    return window;
}

bool DesktopWindow::headset_attached() const noexcept { return stereo_.device_attached(); }

bool DesktopWindow::refuse_fullscreen(bool fullscreen) noexcept {
    if (!fullscreen || !headset_attached()) return false;
    if (refusal_logged_.first()) log_.line(kFullscreenRefusedLog);
    return true;
}

bool DesktopWindow::mirror_size(std::uint32_t& width, std::uint32_t& height) noexcept {
    const auto window = stereo_.attached_window_size();
    if (!window) return false;
    const auto size =
        mirror_back_buffer({static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)}, *window);
    if (!size) return false;
    width  = static_cast<std::uint32_t>(size->width);
    height = static_cast<std::uint32_t>(size->height);
    return true;
}

} // namespace omvr::core::stereo
