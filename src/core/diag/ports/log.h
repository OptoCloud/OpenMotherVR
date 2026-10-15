// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/tuning.h"

#include <cstddef>
#include <format>
#include <span>
#include <string_view>
#include <utility>

namespace omvr::core::diag::ports {

// The project log (mothervr.log). Any thread; no allocation, so the render
// thread may log a rate-limited line.
class Log {
public:
    // Appends one line, prefixed with the time and the process id.
    virtual void line(std::string_view msg) noexcept = 0;
    // As line, rendering into `scratch` instead of a buffer on the stack: for
    // the crash record, after a stack overflow left too little stack.
    virtual void line_in(std::string_view msg, std::span<char> scratch) noexcept = 0;
    // "Dev. Debug Options" is on: extra diagnostics are wanted.
    virtual bool debug_enabled() const noexcept = 0;

protected:
    ~Log() = default;
};

// std::format-style, into a fixed buffer; longer messages are truncated.
template <class... Args>
void logf(Log& log, std::format_string<Args...> fmt, Args&&... args) {
    char buf[kMessageBufferSize];
    auto r = std::format_to_n(buf, sizeof(buf) - 1, fmt, std::forward<Args>(args)...);
    log.line(std::string_view{buf, static_cast<std::size_t>(r.out - buf)});
}

} // namespace omvr::core::diag::ports
