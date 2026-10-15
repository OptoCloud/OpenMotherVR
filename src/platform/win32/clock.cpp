#include "platform/win32/clock.h"

#include <windows.h>

namespace omvr::platform::win32 {

std::chrono::steady_clock::time_point SystemClock::now() const noexcept {
    return std::chrono::steady_clock::now();
}

core::diag::ports::LocalTime SystemClock::local_time() const noexcept {
    SYSTEMTIME t{};
    ::GetLocalTime(&t);
    return {t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds};
}

} // namespace omvr::platform::win32
