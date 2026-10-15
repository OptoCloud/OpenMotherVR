#pragma once

#include "core/input/glyphs.h"
#include "core/input/types.h"

#include <atomic>
#include <span>

namespace omvr::core::input {

// The controller layout in use: the button-prompt glyphs and the controls
// help shown on the options page (04 §4.1, §5.5, §5.7).
class ControlSchemes final {
public:
    explicit ControlSchemes(Glyphs& glyphs) noexcept;

    // Picks the glyphs and the help set for `scheme`. Render thread.
    void set(ControlScheme scheme) noexcept;

    // The controls help lines, function and control separated by a tab
    // (04 §5.7). Read on the game thread; set (render thread) swaps the
    // whole set.
    [[nodiscard]] std::span<const char* const> control_help() const noexcept;

    struct HelpSet;

private:
    Glyphs& glyphs_;
    // The help set is chosen on the render thread (set) and read on the game
    // thread when a menu opens (control_help). One pointer to a whole set
    // crosses, so a reader never pairs one set's entries with another set's
    // count (spec 04 section 3.6).
    std::atomic<const HelpSet*> help_;
};

} // namespace omvr::core::input
