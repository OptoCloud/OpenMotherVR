#include "core/diag/log_format.h"

#include <format>

namespace omvr::core::diag {

// The pid matters because AI.exe spawns a launcher and a game process and
// both run the DLL; without it the log cannot tell them apart.
int format_prefix(char* out, std::size_t cap, const ports::LocalTime& t, std::uint32_t pid) noexcept {
    auto r = std::format_to_n(
        out, cap,
        "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} pid={:05}] ",
        static_cast<unsigned>(t.year), static_cast<unsigned>(t.month),
        static_cast<unsigned>(t.day), static_cast<unsigned>(t.hour),
        static_cast<unsigned>(t.minute), static_cast<unsigned>(t.second),
        static_cast<unsigned>(t.millisecond),
        static_cast<unsigned long>(pid));
    return static_cast<int>(r.out - out);
}

} // namespace omvr::core::diag
