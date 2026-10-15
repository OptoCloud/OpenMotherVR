#include "util/text/command_line_split.h"

namespace omvr::util {

namespace {

constexpr wchar_t kQuote = L'"';

constexpr bool is_separator(wchar_t c) noexcept { return c == L' ' || c == L'\t'; }

} // namespace

std::vector<std::wstring_view> split_command_line(std::wstring_view cmd) {
    std::vector<std::wstring_view> tokens;
    std::size_t i = 0;
    while (i < cmd.size()) {
        while (i < cmd.size() && is_separator(cmd[i])) ++i;
        if (i == cmd.size()) break;
        const bool quoted = cmd[i] == kQuote;
        if (quoted) ++i;
        const std::size_t start = i;
        while (i < cmd.size() && (quoted ? cmd[i] != kQuote : !is_separator(cmd[i]))) ++i;
        tokens.push_back(cmd.substr(start, i - start));
        if (quoted && i < cmd.size()) ++i; // the closing quote
    }
    return tokens;
}

} // namespace omvr::util
