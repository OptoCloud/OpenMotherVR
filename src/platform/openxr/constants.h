#pragma once

#include <cstdint>

// What the mod tells the OpenXR runtime about itself.
namespace omvr::platform::openxr {

inline constexpr const char* kApplicationName = "Alien: Isolation (MotherVR)";
inline constexpr const char* kEngineName      = "MotherVR";
inline constexpr std::uint32_t kApplicationVersion = 1;
inline constexpr std::uint32_t kEngineVersion      = 1;

} // namespace omvr::platform::openxr
