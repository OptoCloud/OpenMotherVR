// OpenMotherVR-Provenance: clean
#include "core/input/controller_rules.h"

#include "core/input/controller_mapping.h"
#include "core/input/gesture_geometry.h"
#include "core/input/glyphs_rule.h"
#include "core/input/tuning.h"

namespace omvr::core::input {

// Built on glyph_family, so the controller types and profile paths are
// named in one place.
ControlScheme scheme_for(ControllerSource source, std::string_view id) noexcept {
    switch (glyph_family(source, id)) {
        case GlyphFamily::Wand: return ControlScheme::Touchpad;
        case GlyphFamily::Frame: return ControlScheme::Frame;
        case GlyphFamily::Index:
            return source == ControllerSource::SteamVrInput ? ControlScheme::Index
                                                            : ControlScheme::Thumbstick;
        case GlyphFamily::Unknown:
        case GlyphFamily::Touch:
        case GlyphFamily::Wmr:
        case GlyphFamily::Generic: return ControlScheme::Thumbstick;
    }
    return ControlScheme::Thumbstick;
}

Controls scheme_controls(ControlScheme scheme, const HandInput& left, const HandInput& right) noexcept {
    return scheme == ControlScheme::Touchpad ? touchpad_controls(left, right)
                                             : thumbstick_controls(left, right);
}

bool right_hand_near_head(const BodyPoses& poses, HeadPosition head) noexcept {
    if (head == HeadPosition::ThisFrame) return hand_near_head(poses, kRightHand, kNearHead);
    BodyPoses held  = poses;
    held.head_valid = true;
    return hand_near_head(held, kRightHand, kNearHead);
}

} // namespace omvr::core::input
