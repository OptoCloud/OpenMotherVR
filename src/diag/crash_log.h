#pragma once

// One log line for a crash: the exception, the module and offset it happened
// in, and the registers, so a report needs only mothervr.log. Windows still
// records the crash as before: the filter that was installed before ours
// runs after it.
namespace mothervr::diag {

// Installs the unhandled-exception filter. Call once, at attach.
void install_crash_log() noexcept;

} // namespace mothervr::diag
