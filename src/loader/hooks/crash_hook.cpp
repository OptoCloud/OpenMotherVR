#include "loader/hooks/crash_hook.h"

#include "loader/context.h"

#include <windows.h>

#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// The unhandled-exception filter. Only the first crash is recorded: a crash
// while logging must not recurse.
LONG WINAPI on_crash(EXCEPTION_POINTERS* info) noexcept { return context().crash.on_crash(info); }

static_assert(std::is_convertible_v<decltype(&on_crash), LPTOP_LEVEL_EXCEPTION_FILTER>);
static_assert(std::is_same_v<LPTOP_LEVEL_EXCEPTION_FILTER, platform::win32::CrashRecorder::Filter>);

} // namespace

void install_crash_hook() noexcept { context().crash.install(&on_crash); }

void reassert_crash_hook() noexcept { context().crash.reassert(&on_crash); }

} // namespace omvr::loader::hooks
