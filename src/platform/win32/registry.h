#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace omvr::platform::win32 {

// Which registry view of a 64-bit Windows a 32-bit process reads: its own
// (redirected to WOW6432Node), or the 32-bit or 64-bit one explicitly.
enum class RegistryView { Default, Bits32, Bits64 };

// The string value `name` under HKEY_LOCAL_MACHINE\`subkey` in `view`, of
// whatever length the registry holds. A REG_EXPAND_SZ value has its
// environment references (%ProgramFiles%...) expanded, as a program that
// reads it would; if expansion fails the value is returned as stored.
// nullopt when the key or value is missing or the value is not a string.
// Any thread.
std::optional<std::wstring> read_machine_string(std::wstring_view subkey, std::wstring_view name,
                                                RegistryView view) noexcept;

} // namespace omvr::platform::win32
