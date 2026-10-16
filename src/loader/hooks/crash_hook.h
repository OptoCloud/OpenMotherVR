// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::loader::hooks {

// Installs the crash record's unhandled-exception filter. Once, at attach.
void install_crash_hook() noexcept;

// Puts the filter back on top if the game replaced it; see
// platform::win32::CrashRecorder::reassert.
void reassert_crash_hook() noexcept;

// With Controls.CrashSafeLog on (read at attach, 22 §14): the log's flush
// policy and the crash event. Once, at attach, after the settings are read.
// With it off, logging stays v1's.
void enable_crash_safe_log() noexcept;

// Starts the mod's log flush thread when crash-safe logging is on. Once,
// on the game thread after attach, outside DllMain (12 §2.3).
void start_log_flusher() noexcept;

} // namespace omvr::loader::hooks
