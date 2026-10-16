// OpenMotherVR-Provenance: clean
// The VR-off hotkey's hold (spec 22 §13).

#include "core/stereo/vr_off_hotkey.h"

#include <doctest/doctest.h>

#include <chrono>

namespace {

using namespace std::chrono_literals;
using omvr::core::stereo::VrOffHotkey;
using Clock = std::chrono::steady_clock;

} // namespace

TEST_SUITE("vr off hotkey") {
    TEST_CASE("fires once the chord has been held for one second") {
        VrOffHotkey key;
        const Clock::time_point t0{10s};
        CHECK_FALSE(key.update(true, t0));
        CHECK_FALSE(key.update(true, t0 + 999ms));
        CHECK(key.update(true, t0 + 1000ms));
        CHECK(key.fired());
    }

    TEST_CASE("fires only once per session") {
        VrOffHotkey key;
        const Clock::time_point t0{10s};
        key.update(true, t0);
        CHECK(key.update(true, t0 + 1s));
        CHECK_FALSE(key.update(true, t0 + 2s));
        CHECK_FALSE(key.update(false, t0 + 3s));
        CHECK_FALSE(key.update(true, t0 + 4s));
        CHECK_FALSE(key.update(true, t0 + 6s));
    }

    TEST_CASE("a release restarts the hold") {
        VrOffHotkey key;
        const Clock::time_point t0{10s};
        key.update(true, t0);
        CHECK_FALSE(key.update(true, t0 + 900ms));
        CHECK_FALSE(key.update(false, t0 + 950ms));
        CHECK_FALSE(key.update(true, t0 + 1000ms));
        CHECK_FALSE(key.update(true, t0 + 1900ms));
        CHECK(key.update(true, t0 + 2000ms));
    }

    TEST_CASE("never fires without the chord") {
        VrOffHotkey key;
        const Clock::time_point t0{10s};
        CHECK_FALSE(key.update(false, t0));
        CHECK_FALSE(key.update(false, t0 + 5s));
        CHECK_FALSE(key.fired());
    }
}
