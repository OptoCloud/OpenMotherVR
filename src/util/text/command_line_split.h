#pragma once

#include <string_view>
#include <vector>

namespace omvr::util {

// The arguments of a Windows command line: separated by spaces or tabs, a
// double-quoted argument taken whole without its quotes. Views into `cmd`.
std::vector<std::wstring_view> split_command_line(std::wstring_view cmd);

} // namespace omvr::util
