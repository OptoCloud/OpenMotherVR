// Planar reflections per eye (07). Analysis: dumps/07_reflections_head_pose.md.

#include "core/stereo/reflection_pass.h"

#include <algorithm>
#include <string_view>

namespace omvr::core::stereo {

namespace {

constexpr std::string_view kRenderedLog = "[stereo] reflections rendered per eye";
constexpr std::string_view kPovLog =
    "[stereo] reflection POV {} flags 0x{:02X} -> 0x{:02X} (state {}, size {}x{})";

} // namespace

void ReflectionPass::note_pov(const ReflectionPovInfo& info) noexcept {
    auto it =
        std::ranges::find_if(seen_, [&](const SeenPov& e) { return e.pov == info.pov || e.pov == nullptr; });
    if (it == seen_.end() || (it->pov != nullptr && it->flags == info.flags) || lines_ >= kReflectionLogLines)
        return;
    ++lines_;
    diag::ports::logf(log_, kPovLog, info.pov, it->pov ? it->flags : -1, info.flags, info.state, info.width,
                      info.height);
    *it = {info.pov, info.flags};
}

bool ReflectionPass::take_capture_request() noexcept {
    // The capture is a DebugOptions diagnostic: with it off the key does
    // nothing.
    const bool down    = settings_.debug.debug && keyboard_.key_down(ports::Key::F10);
    const bool capture = down && !capture_key_down_;
    capture_key_down_  = down;
    return capture;
}

void ReflectionPass::note_rendered() noexcept {
    if (rendered_logged_.first()) log_.line(kRenderedLog);
}

} // namespace omvr::core::stereo
