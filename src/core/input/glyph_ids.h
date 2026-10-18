// OpenMotherVR-Provenance: clean
#pragma once

#include <array>

// The button-prompt ids the mod's UI movies map to VR controller images
// (04 §4.1, 22 §10.2). Each id is registered in the movies in lower case
// with the image DATA/UI/GLYPH_<id>.DDS; scripts/ui_glyphs.py reads the ids
// from this file and draws one image per id (scripts/glyph_art.py), so the
// DLL and the movies always agree. The ids differ from every stock input id
// (`a`, `lb`, ...) by their prefix.
//
// A trailing _L or _R is the hand mark of 22 §10.2: the same design with a
// short bar under its left or right half, for a family that has the same
// control on both hands.
namespace omvr::core::input {

// Face buttons: a ring with the letter inside.
inline constexpr const char* kGlyphA  = "OMVR_A";
inline constexpr const char* kGlyphB  = "OMVR_B";
inline constexpr const char* kGlyphX  = "OMVR_X";
inline constexpr const char* kGlyphY  = "OMVR_Y";
inline constexpr const char* kGlyphAL = "OMVR_A_L";
inline constexpr const char* kGlyphAR = "OMVR_A_R";
inline constexpr const char* kGlyphBL = "OMVR_B_L";
inline constexpr const char* kGlyphBR = "OMVR_B_R";

// Menu button: a ring with three bars.
inline constexpr const char* kGlyphMenu  = "OMVR_MENU";
inline constexpr const char* kGlyphMenuL = "OMVR_MENU_L";
inline constexpr const char* kGlyphMenuR = "OMVR_MENU_R";

// Grips: a light-grey tab with LG / RG.
inline constexpr const char* kGlyphLeftGrip  = "OMVR_LG";
inline constexpr const char* kGlyphRightGrip = "OMVR_RG";

// Right stick click (side view) and the left stick's four directions (top
// view, 22 §7).
inline constexpr const char* kGlyphRightStick     = "OMVR_RSC";
inline constexpr const char* kGlyphLeftStickUp    = "OMVR_LS_U";
inline constexpr const char* kGlyphLeftStickDown  = "OMVR_LS_D";
inline constexpr const char* kGlyphLeftStickLeft  = "OMVR_LS_L";
inline constexpr const char* kGlyphLeftStickRight = "OMVR_LS_R";

// Touchpad regions: a thin ring with a centre dot or a filled quarter
// wedge. Without a mark: v1's touchpad set (04 §4.1).
inline constexpr const char* kGlyphTouchpadCentre  = "OMVR_TP_C";
inline constexpr const char* kGlyphTouchpadUp      = "OMVR_TP_U";
inline constexpr const char* kGlyphTouchpadDown    = "OMVR_TP_D";
inline constexpr const char* kGlyphTouchpadCentreR = "OMVR_TP_C_R";
inline constexpr const char* kGlyphTouchpadUpL     = "OMVR_TP_U_L";
inline constexpr const char* kGlyphTouchpadUpR     = "OMVR_TP_U_R";
inline constexpr const char* kGlyphTouchpadDownL   = "OMVR_TP_D_L";
inline constexpr const char* kGlyphTouchpadDownR   = "OMVR_TP_D_R";
inline constexpr const char* kGlyphTouchpadLeftL   = "OMVR_TP_L_L";
inline constexpr const char* kGlyphTouchpadRightL  = "OMVR_TP_R_L";
// The whole pad filled: a press anywhere on it.
inline constexpr const char* kGlyphTouchpadAnyL = "OMVR_TP_A_L";
inline constexpr const char* kGlyphTouchpadAnyR = "OMVR_TP_A_R";

// The Index trackpad press: an upright filled rounded oblong.
inline constexpr const char* kGlyphTrackpad = "OMVR_TRACKPAD";

// The Steam Frame's left bumper (a thin light-grey tab with LB) and its View
// button (a ring with two overlapping rectangles).
inline constexpr const char* kGlyphLeftBumper = "OMVR_LBUMPER";
inline constexpr const char* kGlyphView       = "OMVR_VIEW";

// The glyph for an action with no pad control, or one outside the game's
// pad codes (04 §3.3, §4.1): a thin ring crossed by one diagonal stroke.
inline constexpr const char* kGlyphUnbound = "OMVR_NONE";

// Every id above (scripts/glyph_art.py reads the literals; the tests check
// that every glyph the rules return is one of these or a stock name).
inline constexpr std::array kAllGlyphIds = {
    kGlyphA,
    kGlyphB,
    kGlyphX,
    kGlyphY,
    kGlyphAL,
    kGlyphAR,
    kGlyphBL,
    kGlyphBR,
    kGlyphMenu,
    kGlyphMenuL,
    kGlyphMenuR,
    kGlyphLeftGrip,
    kGlyphRightGrip,
    kGlyphRightStick,
    kGlyphLeftStickUp,
    kGlyphLeftStickDown,
    kGlyphLeftStickLeft,
    kGlyphLeftStickRight,
    kGlyphTouchpadCentre,
    kGlyphTouchpadUp,
    kGlyphTouchpadDown,
    kGlyphTouchpadCentreR,
    kGlyphTouchpadUpL,
    kGlyphTouchpadUpR,
    kGlyphTouchpadDownL,
    kGlyphTouchpadDownR,
    kGlyphTouchpadLeftL,
    kGlyphTouchpadRightL,
    kGlyphTouchpadAnyL,
    kGlyphTouchpadAnyR,
    kGlyphTrackpad,
    kGlyphLeftBumper,
    kGlyphView,
    kGlyphUnbound,
};

} // namespace omvr::core::input
