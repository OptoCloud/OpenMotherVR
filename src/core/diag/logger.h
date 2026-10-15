#pragma once

#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/diag/ports/log_sink.h"
#include "core/diag/tuning.h"

#include <atomic>
#include <cstdint>
#include <span>
#include <string_view>

namespace omvr::core::diag {

// The project logger: each line is `[yyyy-MM-dd hh:mm:ss.fff pid=PPPPP]
// <message>\r\n`, handed to the sink in one piece. Any thread; no
// allocation, so a rate-limited line may come from the render thread.
class Logger final : public ports::Log {
public:
    Logger(const ports::Clock& clock, ports::LogSink& sink, std::uint32_t pid) noexcept
        : clock_(clock), sink_(sink), pid_(pid) {}

    void line(std::string_view msg) noexcept override;
    void line_in(std::string_view msg, std::span<char> scratch) noexcept override;
    bool debug_enabled() const noexcept override;

    // The setting debug_enabled() reports (Settings.DebugOptions), which may
    // change while the game runs. Release/acquire: a reader that sees the
    // pointer sees the settings it points into.
    void set_debug_flag(const bool* flag) noexcept { debug_flag_.store(flag, std::memory_order_release); }

private:
    const ports::Clock&      clock_;
    ports::LogSink&          sink_;
    std::uint32_t            pid_;
    std::atomic<const bool*> debug_flag_{nullptr};
};

} // namespace omvr::core::diag
