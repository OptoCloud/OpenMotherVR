#pragma once

namespace omvr::loader::hooks {

// Installs the crash record's unhandled-exception filter. Once, at attach.
void install_crash_hook() noexcept;

// Puts the filter back on top if the game replaced it; see
// platform::win32::CrashRecorder::reassert.
void reassert_crash_hook() noexcept;

} // namespace omvr::loader::hooks
