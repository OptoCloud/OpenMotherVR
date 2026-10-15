#pragma once

#include "core/diag/ports/clock.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

// The line format of mothervr.log (spec 11).
namespace omvr::core::diag {

// Every line ends so: the log is read with Windows tools.
inline constexpr std::string_view kLineEnding = "\r\n";

// Writes the line prefix `[yyyy-MM-dd hh:mm:ss.fff pid=PPPPP] ` for `t` and
// `pid` into `out` (cut to fit, not terminated). Returns its length. Pure;
// no allocation.
std::size_t format_prefix(std::span<char> out, const ports::LocalTime& t, std::uint32_t pid) noexcept;

// Renders the whole line, prefix, `msg` (cut to fit) and kLineEnding, into
// `out`. Returns its length, or 0 when `out` cannot hold the prefix and the
// line ending.
std::size_t format_line(std::span<char> out, const ports::LocalTime& t, std::uint32_t pid,
                        std::string_view msg) noexcept;

// A path as UTF-8 for a log line. Never throws; empty if the path cannot be
// converted (a name that is not valid UTF-16).
std::string path_text(const std::filesystem::path& path) noexcept;

} // namespace omvr::core::diag
