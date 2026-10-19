#include "core/ui/display_reset.h"

#include "core/ui/runtime_menu.h"

#include <utility>

namespace omvr::core::ui {

namespace {

// The log names a changed setting by its mothervr.json key path.
const char* changed(bool differs, const char* path) noexcept { return differs ? path : ""; }

} // namespace

TargetSettings target_settings(const config::Settings& cfg) noexcept {
    return {cfg.rendering.pixel_scale, cfg.rendering.mirror_view_type, cfg.rendering.symmetric_mirror_view};
}

void DisplayReset::seed(const config::Settings& cfg) noexcept {
    if (!built_) built_ = target_settings(cfg);
}

void DisplayReset::reset_if_changed(const config::Settings& cfg) noexcept {
    seed(cfg); // first call: what the targets were made with
    TargetSettings& built    = *built_;
    const TargetSettings now = target_settings(cfg);
    if (now == built) return;
    const TargetSettings was = std::exchange(built, now);
    // With no VR this run there are no stereo targets to resize, and a
    // rebuild would only freeze the picture.
    if (!signals_.can_reset_display() || running_runtime(cfg, runtime_) == config::VRRuntime::None) return;
    signals_.request_display_reset();
    diag::ports::logf(
        log_, "[script_bridge] display reset raised; changed:{}{}{}",
        changed(now.pixel_scale != was.pixel_scale, " Rendering.PixelScale"),
        changed(now.mirror_view_type != was.mirror_view_type, " Rendering.MirrorViewType"),
        changed(now.symmetric_mirror_view != was.symmetric_mirror_view, " Rendering.SymmetricMirrorView"));
}

void DisplayReset::reset_after_vr_off() noexcept {
    if (!signals_.can_reset_display()) return;
    signals_.request_display_reset();
    log_.line("[script_bridge] display reset raised; VR turned off, targets return to the window size");
}

} // namespace omvr::core::ui
