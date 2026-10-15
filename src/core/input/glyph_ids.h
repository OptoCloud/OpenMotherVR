#pragma once

// The button-prompt ids the mod's UI movies map to VR controller images
// (04 §4.1).
namespace omvr::core::input {

inline constexpr const char* kGlyphTouchpadDown   = "VR_TP_D";
inline constexpr const char* kGlyphTouchpadCentre = "VR_TP_C";
inline constexpr const char* kGlyphTouchpadUp     = "VR_TP_U";
inline constexpr const char* kGlyphMenu           = "VR_MENU";
inline constexpr const char* kGlyphA              = "VR_A";
inline constexpr const char* kGlyphRightStick     = "VR_RSC";
inline constexpr const char* kGlyphB              = "VR_B";
inline constexpr const char* kGlyphY              = "VR_Y";
inline constexpr const char* kGlyphX              = "VR_X";
inline constexpr const char* kGlyphLeftGrip       = "VR_LG";
inline constexpr const char* kGlyphRightGrip      = "VR_RG";

// The glyph for an action with no pad control, or one outside the game's
// pad codes (04 §3.3, §4.1). The UI draws it under the menu id.
inline constexpr const char* kGlyphUnbound = kGlyphMenu;

} // namespace omvr::core::input
