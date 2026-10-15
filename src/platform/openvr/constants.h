#pragma once

#include <openvr.h>

#include <cstddef>
#include <cstdint>

// OpenVR values the SDK headers do not name, and the files and folders of a
// SteamVR install the mod relies on.
namespace omvr::platform::openvr {

// Legacy controller state button bits.
inline constexpr std::uint64_t kButtonMenu     = 1ull << vr::k_EButton_ApplicationMenu;
inline constexpr std::uint64_t kButtonGrip     = 1ull << vr::k_EButton_Grip;
inline constexpr std::uint64_t kButtonTouchpad = 1ull << vr::k_EButton_SteamVR_Touchpad;

// Bumped whenever actions are added (2: the hand poses). SteamVR reads the
// manifest we rewrite each launch, so new actions and the default bindings
// for them arrive by themselves; the number marks which manifest a binding
// file was made for, so a binding a player saved from an older version can
// be told apart in SteamVR's binding UI (it lacks the new actions, which
// then read inactive: see Actions::hand_poses()).
inline constexpr int kManifestVersion = 2;

// Where SteamVR publishes its install location (03 §6.4):
// %LOCALAPPDATA%\openvr\openvrpaths.vrpath, a JSON file whose "runtime"
// array names the runtime root.
inline constexpr const wchar_t* kLocalAppDataVariable = L"LOCALAPPDATA";
inline constexpr const char*    kPathsDirectory       = "openvr";
inline constexpr const char*    kPathsFileName        = "openvrpaths.vrpath";
inline constexpr const char*    kPathsRuntimeKey      = "runtime";

// Inside the runtime root: the x86 loader in bin\win32, and vrclient.dll,
// which the loader loads, in bin (03 §6.4).
inline constexpr const char* kBinDirectory    = "bin";
inline constexpr const char* kX86Directory    = "win32";
inline constexpr const char* kLoaderFileName  = "openvr_api.dll";
inline constexpr const char* kVrClientFileName = "vrclient.dll";

// The action manifest and the binding files written next to it
// (04 §5.8.2): bindings_<controller type>.json.
inline constexpr const char* kManifestFileName = "actions.json";
inline constexpr const char* kBindingFilePrefix = "bindings_";
inline constexpr const char* kBindingFileSuffix = ".json";
// Indentation of the JSON files written, for players who read them.
inline constexpr int kJsonIndent = 2;

// Buffer for a tracked-device string property (manufacturer, model,
// controller type); longer values are cut.
inline constexpr std::size_t kPropertyBufferSize = 64;

// Eye bounds that show nothing: in screen frames the compositor still gets
// an eye texture every frame, and these bounds make the 3D view black
// (03 §7, "around the screen").
inline constexpr vr::VRTextureBounds_t kNoImageBounds{0.0f, 0.0f, 1e-5f, 1e-5f};

} // namespace omvr::platform::openvr
