#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/ui/ports/game_signals.h"
#include "core/ui/ports/runtime_state.h"

#include <optional>

namespace omvr::core::ui {

// The settings the game's render targets were last built with. The original
// raises the reset flag on every menu open; a rebuild stalls the game for
// about 0.2 s, so it is raised only when one of these actually changed.
struct TargetSettings {
    float pixel_scale;
    config::MirrorViewType mirror_view_type;
    bool symmetric_mirror_view;
    bool operator==(const TargetSettings&) const = default;
};

TargetSettings target_settings(const config::Settings& cfg) noexcept;

// Raises the game's display reset when a setting the render targets are
// built with changed, so the change takes effect without a restart; an
// unchanged push leaves it alone (a rebuild is a visible freeze). Game
// thread.
class DisplayReset {
public:
    DisplayReset(ports::GameSignals& signals, ports::RuntimeState& runtime, diag::ports::Log& log) noexcept
        : signals_(signals), runtime_(runtime), log_(log) {}

    // Records `cfg` as what the targets were made with, unless something is
    // recorded already. At attach (the settings loaded), and by a command
    // before it changes a setting, so that change is seen as one.
    void seed(const config::Settings& cfg) noexcept;

    // The first call, when nothing was seeded, records what the targets
    // were made with.
    void reset_if_changed(const config::Settings& cfg) noexcept;

    // VR was turned off by the hotkey (22 §13): the game rebuilds its
    // targets at the window's size, once, instead of keeping the stereo
    // size until the next resize. Game or render thread.
    void reset_after_vr_off() noexcept;

private:
    ports::GameSignals& signals_;
    ports::RuntimeState& runtime_;
    diag::ports::Log& log_;
    std::optional<TargetSettings> built_;
};

} // namespace omvr::core::ui
