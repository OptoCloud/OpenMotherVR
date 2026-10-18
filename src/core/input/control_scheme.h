#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/input/glyphs.h"
#include "core/input/glyphs_rule.h"
#include "core/input/types.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <string_view>

namespace omvr::core::input {

// The controls help lines for one push, function and control separated by a
// tab (04 §5.7, 22 §10.3): the set's rows, then the rows of the v2 features
// that are on. A value, so the game thread holds no pointer into state the
// render thread replaces.
class HelpRows {
public:
    static constexpr std::size_t kCapacity = 32;

    void push_back(const char* line) noexcept {
        if (size_ < kCapacity) lines_[size_++] = line;
    }
    void append(std::span<const char* const> lines) noexcept {
        for (const char* line : lines) push_back(line);
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] const char* operator[](std::size_t i) const noexcept { return lines_[i]; }
    [[nodiscard]] const char* front() const noexcept { return lines_[0]; }
    [[nodiscard]] const char* back() const noexcept { return lines_[size_ - 1]; }
    [[nodiscard]] const char* const* begin() const noexcept { return lines_.data(); }
    [[nodiscard]] const char* const* end() const noexcept { return lines_.data() + size_; }

private:
    std::array<const char*, kCapacity> lines_{};
    std::size_t size_ = 0;
};

// The controller layout in use: the button-prompt glyphs and the controls
// help shown on the options page (04 §4.1, §5.5, §5.7; 22 §7, §10).
class ControlSchemes final {
public:
    // v1 only: every Controls key at its default, nothing logged (tests).
    explicit ControlSchemes(Glyphs& glyphs) noexcept;
    // `controls` is read live: Controls.ControllerGlyphs at each controller
    // identification, Controls.DpadEmulation on every refresh, and the v2
    // help rows' keys at each push.
    ControlSchemes(Glyphs& glyphs, const config::ControlsSettings& controls, diag::ports::Log& log) noexcept;

    // v1's choice: the glyphs and the help set for `scheme` (04 §5.5).
    // Render thread.
    void set(ControlScheme scheme) noexcept;

    // A hand's controller was identified (22 §10.1): with
    // Controls.ControllerGlyphs on, the glyphs follow each hand's family and
    // the help the right hand's. Logged when a family changes. Render thread.
    void identify(std::size_t hand, ControllerSource source, std::string_view id) noexcept;

    // Applies a change of Controls.DpadEmulation to the d-pad glyphs (22 §7).
    // Cheap when nothing changed; render thread, every controls write.
    void refresh() noexcept;

    // The controls help lines (04 §5.7, 22 §10.3). Game thread, at a push.
    [[nodiscard]] HelpRows control_help() const noexcept;

    struct HelpSet;

private:
    // Writes every pad code's glyph from the current state. Render thread.
    void apply() noexcept;
    [[nodiscard]] const HelpSet& v2_help_set() const noexcept;

    Glyphs& glyphs_;
    const config::ControlsSettings& controls_;
    diag::ports::Log* log_;

    // Render thread.
    ControlScheme scheme_    = ControlScheme::Touchpad;
    bool scheme_chosen_      = false;
    ControllerSource source_ = ControllerSource::SteamVrInput;
    std::array<GlyphFamily, 2> families_{GlyphFamily::Unknown, GlyphFamily::Unknown};
    bool families_on_ = false; // Controls.ControllerGlyphs, as of the last identification
    bool dpad_on_     = false; // Controls.DpadEmulation, as the glyphs show it
    bool v2_applied_  = false; // the glyphs shown are v2's (families or the d-pad)

    // The help set is chosen on the render thread and read on the game
    // thread when a menu opens (control_help). One pointer to a whole set
    // crosses, so a reader never pairs one set's entries with another set's
    // count (spec 04 section 3.6).
    std::atomic<const HelpSet*> help_;
};

} // namespace omvr::core::input
