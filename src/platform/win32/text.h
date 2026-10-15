#pragma once

#include <filesystem>
#include <string>

namespace omvr::platform::win32 {

// A path as UTF-8, for logs and for APIs that take UTF-8 (OpenVR). Never
// throws, unlike path::string(), which fails on characters outside the ANSI
// code page (a user name, an install path).
std::string utf8(const std::filesystem::path& path) noexcept;

} // namespace omvr::platform::win32
