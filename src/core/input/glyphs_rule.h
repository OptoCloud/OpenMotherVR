// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/types.h"

#include <string_view>

// Which button glyph a prompt shows (04 §4.1, 22 §7 and §10): v1's two sets
// chosen by the control scheme, and v2's glyphs per controller family and
// hand.
namespace omvr::core::input {

// The controller families of 22 §10.1, each with its own glyphs.
enum class GlyphFamily { Unknown, Index, Touch, Wmr, Wand, Frame, Generic };

// The family a runtime's identification names (22 §10.1): a SteamVR
// controller type, or an OpenXR interaction profile path. Unknown for an
// empty id (no device, no profile bound); every controller is a wand on
// OpenVR's legacy path (04 §5.8.3).
[[nodiscard]] GlyphFamily glyph_family(ControllerSource source, std::string_view id) noexcept;

// The family's name for the log.
[[nodiscard]] const char* family_name(GlyphFamily family) noexcept;

// The glyph for stock pad code `code` with these controllers in the left and
// right hands (22 §10.2): a control of the left hand takes the left
// controller's design, one of the right hand the right controller's. A hand
// not identified yet counts as holding the other hand's family. nullptr when
// neither hand is identified, so the caller keeps v1's set. A code without a
// VR glyph gives the game's own name. `dpad_emulation`: Controls.DpadEmulation
// (22 §7).
[[nodiscard]] const char* family_glyph(GlyphFamily left, GlyphFamily right, ControllerSource source, int code,
                                       bool dpad_emulation) noexcept;

// v1's glyph for stock pad code `code` under `scheme` (04 §4.1), and the
// emulated d-pad's glyphs while `dpad_emulation` is on (22 §7).
[[nodiscard]] const char* scheme_glyph(ControlScheme scheme, int code, bool dpad_emulation) noexcept;

} // namespace omvr::core::input
