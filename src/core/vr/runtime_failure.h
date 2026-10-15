#pragma once

#include <cstdint>
#include <string>

namespace omvr::core::vr {

// Why the chosen runtime failed to start: the runtime's error text and code,
// for the player-facing message (03 §6.4, 13 §5.3).
struct InitFailure {
    std::string what;
    std::int32_t code = 0;
};

// The start-up failure of this run, reported by the runtime that failed and
// read by the stereo device for the player's message. Game thread (the
// runtime starts in CreateDevice).
class RuntimeFailure {
public:
    void report(std::string what, std::int32_t code) noexcept;
    const InitFailure& last() const noexcept { return last_; }

private:
    InitFailure last_;
};

} // namespace omvr::core::vr
