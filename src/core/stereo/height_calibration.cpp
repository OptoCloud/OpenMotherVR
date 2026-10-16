// OpenMotherVR-Provenance: clean
#include "core/stereo/height_calibration.h"

#include "core/config/tuning.h"
#include "core/stereo/tuning.h"

#include <chrono>
#include <cmath>
#include <string_view>

namespace omvr::core::stereo {

namespace {

using diag::ports::logf;

constexpr std::string_view kLogRequested = "[stereo] height calibration requested: measuring in {} s";
constexpr std::string_view kLogWaiting =
    "[stereo] height calibration waiting: no valid head pose with the headset in use";
constexpr std::string_view kLogTaken = "[stereo] standing eye height calibrated: {:.2f} m{}";
constexpr std::string_view kLogOutOfRange =
    "[stereo] standing eye height not taken: {:.2f} m is outside {:.2f} to {:.2f} m; the stored value stays";
constexpr std::string_view kLogNoFloor =
    "[stereo] standing eye height not taken: no floor reference on this runtime";
constexpr std::string_view kAutomatic = " (automatic, at a recenter in standing mode)";

} // namespace

bool calibration_in_range(float height_m) noexcept {
    return height_m >= config::kStandingEyeHeightMin && height_m <= config::kStandingEyeHeightMax;
}

void HeightCalibration::request(TimePoint now) noexcept {
    state_ = CalibrationState::Counting;
    due_   = now + kCalibrationDelay;
    logf(log_, kLogRequested, std::chrono::seconds(kCalibrationDelay).count());
}

std::optional<float> HeightCalibration::update(TimePoint now, const CalibrationReading& reading) noexcept {
    if (state_ == CalibrationState::Counting) {
        if (now < due_) return std::nullopt;
        state_ = CalibrationState::Waiting;
        if (reading.has_floor && !(reading.in_use && reading.head_height_m)) log_.line(kLogWaiting);
    }
    if (state_ != CalibrationState::Waiting) return std::nullopt;
    if (!reading.has_floor) {
        state_ = CalibrationState::NoFloor;
        log_.line(kLogNoFloor);
        return std::nullopt;
    }
    // 22 §6.2: an invalid pose never calibrates; the request stays pending.
    if (!reading.in_use || !reading.head_height_m) return std::nullopt;
    return take(*reading.head_height_m, false);
}

std::optional<float> HeightCalibration::take_automatic(float height_m) noexcept {
    return take(height_m, true);
}

std::optional<float> HeightCalibration::take(float height_m, bool automatic) noexcept {
    height_m_ = height_m;
    if (!std::isfinite(height_m) || !calibration_in_range(height_m)) {
        state_ = CalibrationState::OutOfRange;
        logf(log_, kLogOutOfRange, height_m, config::kStandingEyeHeightMin, config::kStandingEyeHeightMax);
        return std::nullopt;
    }
    state_ = CalibrationState::Taken;
    logf(log_, kLogTaken, height_m, automatic ? kAutomatic : std::string_view{});
    return height_m;
}

CalibrationStatus HeightCalibration::status(TimePoint now) const noexcept {
    CalibrationStatus s{.state = state_, .seconds_left = 0, .height_m = height_m_};
    if (state_ == CalibrationState::Counting) {
        // 3, 2, 1: whole seconds still to wait, rounded up.
        const auto left  = due_ > now ? due_ - now : TimePoint::duration::zero();
        const auto whole = std::chrono::ceil<std::chrono::seconds>(left).count();
        s.seconds_left   = static_cast<int>(whole > 0 ? whole : 1);
    }
    return s;
}

} // namespace omvr::core::stereo
