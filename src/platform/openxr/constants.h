#pragma once

#include <cstddef>
#include <cstdint>

// What the mod tells the OpenXR runtime about itself, and the names and
// sizes of the OpenXR conventions it relies on that are not in the SDK's
// headers (or that headers outside this adapter must not include).
namespace omvr::platform::openxr {

inline constexpr const char* kApplicationName = "Alien: Isolation (MotherVR)";
inline constexpr const char* kEngineName      = "MotherVR";
inline constexpr std::uint32_t kApplicationVersion = 1;
inline constexpr std::uint32_t kEngineVersion      = 1;

// The loader's runtime registration (09 §1): the active runtime's manifest
// path under HKLM, in either registry view.
inline constexpr const wchar_t* kActiveRuntimeKey   = L"SOFTWARE\\Khronos\\OpenXR\\1";
inline constexpr const wchar_t* kActiveRuntimeValue = L"ActiveRuntime";
// The variable (and loader property) that names the runtime manifest.
inline constexpr const char*    kRuntimeJsonName     = "XR_RUNTIME_JSON";
inline constexpr const wchar_t* kRuntimeJsonVariable = L"XR_RUNTIME_JSON";

// The controller actions (04 §5.8.4): one action set, created once per session.
inline constexpr const char* kActionSetName          = "gameplay";
inline constexpr const char* kActionSetLocalizedName = "Gameplay";

// XR_MAX_SYSTEM_NAME_SIZE and XR_MAX_RUNTIME_NAME_SIZE, for SystemInfo,
// whose header does not include OpenXR (checked against the SDK in
// instance.cpp).
inline constexpr std::size_t kSystemNameSize  = 256;
inline constexpr std::size_t kRuntimeNameSize = 128;

} // namespace omvr::platform::openxr
