#pragma once

#include <string_view>

// Names the player and the mod's UI movies see, kept as the original has
// them so existing installs, menus and habits keep working (spec 06, 14).
namespace omvr::core::ui {

// The caption of every message box.
inline constexpr const wchar_t* kCaption = L"MotherVR";

// The game's data directory, the stock UI archive in it, the mod's archive
// built from it, and the sidecar recording which UI.PAK that was (spec 14
// §5.8).
inline constexpr const wchar_t* kDataDirectory    = L"DATA";
inline constexpr const wchar_t* kStockArchiveName = L"UI.PAK";
inline constexpr const wchar_t* kModArchiveName   = L"MOTHER.PAK";
inline constexpr const wchar_t* kSidecarSuffix    = L".src";

// Console commands of the options page start with this; any other command
// is the game's own (spec 06 §4.1).
inline constexpr std::string_view kCommandPrefix = "mvr_";

// The options page's runtime entries, the integers the menu sends with
// mvr_vrruntime and is given in setMotherSetVRRuntime ("Disable VR", then
// the original's "Use Oculus", now OpenXR, then "Use SteamVR").
enum class RuntimeEntry : int { Off = 0, OpenXr = 1, OpenVr = 2 };

// The script invoke methods a settings push calls on the menu (spec 06 §4.3).
namespace script {
inline constexpr const char* kOptionsEnabled       = "setMotherVROptionsEnabled";
inline constexpr const char* kRuntime              = "setMotherSetVRRuntime";
inline constexpr const char* kDebug                = "setMotherDebug";
inline constexpr const char* kSkipIntro            = "setMotherSkipIntro";
inline constexpr const char* kMirrorView           = "setMotherMirrorView";
inline constexpr const char* kMirrorViewSymmetric  = "setMotherMirrorViewSym";
inline constexpr const char* kPixelScale           = "setMotherPixelScale";
inline constexpr const char* kClipBlinding         = "setMotherClipBlinding";
inline constexpr const char* kSnapRotation         = "setMotherSnapRotation";
inline constexpr const char* kSnapRotationSegments = "setMotherSnapRotationSegments";
inline constexpr const char* kInputRelativeWalking = "setMotherInputRelativeWalking";
inline constexpr const char* kInputStrafing        = "setMotherInputStrafing";
inline constexpr const char* kToggleGrips          = "setMotherToggleGrips";
inline constexpr const char* kToggleCrouch         = "setMotherToggleCrouch";
// One row of the controls help: an action and its control.
inline constexpr const char* kAddControlRow = "addMotherViewControl";
} // namespace script

} // namespace omvr::core::ui
