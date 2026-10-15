#include "util/text/command_line_split.h"

namespace omvr::util {

std::vector<std::wstring_view> split_command_line(std::wstring_view cmd) {
    std::vector<std::wstring_view> tokens;
    std::size_t i = 0;
    while (i < cmd.size()) {
        while (i < cmd.size() && (cmd[i] == L' ' || cmd[i] == L'\t')) ++i;
        if (i == cmd.size()) break;
        const bool quoted = cmd[i] == L'"';
        if (quoted) ++i;
        const std::size_t start = i;
        while (i < cmd.size() && (quoted ? cmd[i] != L'"' : (cmd[i] != L' ' && cmd[i] != L'\t'))) ++i;
        tokens.push_back(cmd.substr(start, i - start));
        if (quoted && i < cmd.size()) ++i;   // the closing quote
    }
    return tokens;
}

} // namespace omvr::util
