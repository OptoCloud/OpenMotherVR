#pragma once

#include <cstdint>

namespace omvr::util {

// Counts calls and says when one falls on a period: "every N frames". The
// first call is call 1. One thread.
class FramePeriod {
public:
    explicit constexpr FramePeriod(std::uint32_t period) noexcept : period_(period) {}

    bool tick() noexcept { return period_ != 0 && ++count_ % period_ == 0; }

private:
    std::uint32_t period_;
    std::uint32_t count_ = 0;
};

} // namespace omvr::util
