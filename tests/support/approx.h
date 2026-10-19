// OpenMotherVR-Provenance: clean
#pragma once

// Tolerances the tests compare single-precision results with, and the
// three-component check over them, so a test states its tolerance once
// instead of each file redefining it.

#include "util/math/vec.h"

#include <doctest/doctest.h>

namespace omvr::tests {

// doctest::Approx's epsilon (the allowed difference is eps times 1 plus the
// larger magnitude) for a result a few float operations away from its
// expected value.
inline constexpr float kEps = 1e-4f;

// Each component of `actual` equals that of `expected` within `eps`, as
// doctest::Approx(expected).epsilon(eps) compares.
inline void check_near(const util::Vec3& actual, const util::Vec3& expected, float eps = kEps) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(eps));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(eps));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(eps));
}

} // namespace omvr::tests
