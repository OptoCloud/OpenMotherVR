#pragma once

namespace omvr::util {

// Strong unit types for values whose units a caller could mix up (a
// distance passed where an angle is expected). Explicit in, explicit out;
// the comparisons are the only arithmetic they need so far.
template <class Tag>
struct Unit {
    float value = 0.0f;

    constexpr Unit() noexcept = default;
    explicit constexpr Unit(float v) noexcept : value(v) {}

    friend constexpr bool operator==(Unit, Unit) noexcept  = default;
    friend constexpr auto operator<=>(Unit, Unit) noexcept = default;
};

struct MetresTag {};
struct RadiansTag {};

using Metres  = Unit<MetresTag>;
using Radians = Unit<RadiansTag>;

} // namespace omvr::util
