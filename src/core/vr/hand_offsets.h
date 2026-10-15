#pragma once

#include "core/diag/tuning.h"

#include <array>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>

namespace omvr::core::vr {

// Dev. Debug Options: where `hand` is relative to `head`, in the head's own
// frame (+X right, +Y up, -Z forward), metres, so moving a controller can be
// checked against the log (11 §4.2). Both are runtime-convention 3x4 poses
// (unconverted, unscaled).
inline void head_local_offset(const float (&head)[3][4], const float (&hand)[3][4],
                              float (&out)[3]) noexcept {
    constexpr int kAxes           = 3;
    constexpr int kPositionColumn = 3;
    float d[kAxes];
    for (int r = 0; r < kAxes; ++r) d[r] = hand[r][kPositionColumn] - head[r][kPositionColumn];
    // The world offset in the head's basis: the transposed rotation applied.
    for (int c = 0; c < kAxes; ++c) out[c] = head[0][c] * d[0] + head[1][c] * d[1] + head[2][c] * d[2];
}

// The periodic hands line of Dev. Debug Options, built in a fixed buffer:
// it is written on the render thread, which must not allocate (12 §6.1).
//   "<runtime>: hands (head-local m, +x right +y up -z fwd): L (...) R (...)"
class HandLogLine {
public:
    // `prefix` starts the line, for example
    // "openvr: hands (head-local m, +x right +y up -z fwd):".
    explicit HandLogLine(std::string_view prefix) { append("{}", prefix); }

    // Starts hand `hand`'s entry (0 left, 1 right).
    void begin_hand(std::size_t hand) { append("{}", hand == 0 ? " L " : " R "); }
    // The hand's offset from the head (head_local_offset).
    void offset(const float (&d)[3]) { append("({:+.2f} {:+.2f} {:+.2f})", d[0], d[1], d[2]); }

    // Any other text, cut to fit the buffer.
    template <class... Args>
    void append(std::format_string<Args...> fmt, Args&&... args) {
        const auto r =
            std::format_to_n(buf_.data() + size_, buf_.size() - size_, fmt, std::forward<Args>(args)...);
        size_ = static_cast<std::size_t>(r.out - buf_.data());
    }

    std::string_view text() const noexcept { return {buf_.data(), size_}; }

private:
    std::array<char, diag::kMessageBufferSize> buf_{};
    std::size_t size_ = 0;
};

} // namespace omvr::core::vr
