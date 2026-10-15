#pragma once

// One log line for a crash: the exception, the module and offset it happened
// in, and the registers, so a report needs only mothervr.log. Windows still
// records the crash as before: the filter that was installed before ours
// runs after it.
namespace mothervr::diag {

// Installs the unhandled-exception filter. Call once, at attach.
void install_crash_log() noexcept;

// Puts the filter back on top if something installed another one after
// attach (the game or its C runtime), which would otherwise silently drop
// the crash record. The filter found there is kept and runs after the
// record. Logs once per change. Cheap (one system call), but not for the
// render thread's every frame: call it at points the game passes now and
// then, such as a menu opening or the first frame.
void reassert_crash_log() noexcept;

} // namespace mothervr::diag
