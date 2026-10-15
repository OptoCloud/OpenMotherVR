#pragma once

#include <optional>
#include <string>
#include <string_view>

// The process environment. Windows keeps one copy and the C runtime of each
// module another; a library loaded later (the OpenXR loader) reads its own
// CRT's copy or the process's, so both are named here.
namespace omvr::platform::win32 {

// The variable `name` in the process environment (GetEnvironmentVariable);
// nullopt when it is not set or cannot be read.
std::optional<std::wstring> environment_variable(std::wstring_view name);

// Whether `name` is set in the process environment, without reading it.
bool has_environment_variable(std::wstring_view name) noexcept;

// The variable `name` in this module's C runtime environment (_wdupenv_s),
// nullopt when it is not set.
std::optional<std::wstring> crt_environment_variable(std::wstring_view name);

// Sets `name` in this module's C runtime environment and the process's
// (_wputenv_s does both). False if it failed.
bool set_crt_environment_variable(std::wstring_view name, std::wstring_view value) noexcept;

} // namespace omvr::platform::win32
