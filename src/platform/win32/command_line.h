#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace omvr::platform::win32 {

// The process's command line as Windows holds it (GetCommandLineW). Safe
// under the loader lock.
std::wstring_view process_command_line() noexcept;

// The command line split as the shell does (CommandLineToArgvW), program
// name first; empty if it cannot be split.
std::vector<std::wstring> process_arguments();

} // namespace omvr::platform::win32
