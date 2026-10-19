// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/diag/ports/log_flush.h"
#include "core/diag/tuning.h"
#include "platform/win32/constants.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <format>
#include <span>
#include <utility>

struct _EXCEPTION_POINTERS;

// One log line for a crash: the exception, the module and offset it happened
// in, and the registers, so a report needs only mothervr.log. Windows still
// records the crash as before: the filter that was installed before ours
// runs after it.
namespace omvr::platform::win32 {

class CrashRecorder {
public:
    // An unhandled-exception filter (LPTOP_LEVEL_EXCEPTION_FILTER).
    using Filter = long(__stdcall*)(_EXCEPTION_POINTERS* info);

    explicit CrashRecorder(core::diag::ports::Log& log) noexcept : log_(log) {}

    // Renders Controls.CrashSafeLog's crash event (22 §14) into `out` and
    // returns its length; on the faulting thread, without allocating.
    using StateRenderer = std::size_t (*)(std::span<char> out) noexcept;

    // Installs `ours` as the unhandled-exception filter. Call once, at attach.
    void install(Filter ours) noexcept;

    // Puts `ours` back on top if something installed another one after
    // attach (the game or its C runtime), which would otherwise silently drop
    // the crash record. The filter found there is kept and runs after the
    // record, before those found earlier (each chains to the one it
    // replaced, which was ours, so they run newest first and none drops
    // out). Logs once per change. Cheap (one system call), but not for the
    // render thread's every frame: call it at points the game passes now and
    // then, such as a menu opening or the first frame.
    void reassert(Filter ours) noexcept;

    // The filter's work: the record (first crash only), then the chain. On
    // the faulting thread, in whatever state the process is.
    long on_crash(_EXCEPTION_POINTERS* info) noexcept;

    // Controls.CrashSafeLog (22 §14): after the record, one more event with
    // the mod's state, then the log flushed to disk before the exception is
    // passed on, since the process may end as soon as it is. At attach.
    void enable_crash_safe(StateRenderer render, core::diag::ports::LogFlush& flush) noexcept;

private:
    template <class... Args>
    void record(std::format_string<Args...> fmt, Args&&... args) noexcept;
    void write_record(const _EXCEPTION_POINTERS& info) noexcept;
    void write_stack(const _EXCEPTION_POINTERS& info) noexcept;
    void write_state_and_flush() noexcept;
    long chain(_EXCEPTION_POINTERS* info) noexcept;
    bool is_foreign(Filter filter) const noexcept;
    void add_foreign(Filter filter) noexcept;

    core::diag::ports::Log& log_;
    Filter previous_ = nullptr;
    std::atomic<bool> logged_{false};
    // The filters others (the game's C runtime, an overlay) installed over
    // ours after attach, in the order reassert found them. Ours is put back
    // on top and these run after the record, newest first, so neither the
    // record nor anyone's own handling is lost. Each slot is written once,
    // before foreign_count_ (release) makes it visible; a full list keeps
    // its first entries (the C runtime's) and the newest in the last slot.
    static constexpr std::size_t kMaxForeignFilters = 4;
    std::array<std::atomic<Filter>, kMaxForeignFilters> foreign_{};
    std::atomic<std::size_t> foreign_count_{0};
    // The thread currently running the foreign filters, and how many of
    // them it has entered: when one chains to the filter it replaced (ours),
    // the nested call goes on to the next older one, then to previous_,
    // instead of looping back.
    std::atomic<unsigned long> chaining_thread_{0};
    std::atomic<std::size_t> chain_entered_{0};
    std::atomic_flag reassert_busy_;
    // Set once at attach with Controls.CrashSafeLog on; null keeps v1's record.
    std::atomic<StateRenderer> render_state_{nullptr};
    std::atomic<core::diag::ports::LogFlush*> flush_{nullptr};

    // The record is rendered in storage allocated once at attach, not on the
    // faulting thread's stack: after a stack overflow only the guard region
    // is left, too little for a line buffer plus two MAX_PATH names.
    // logged_ makes the record a one-off, so one set of buffers is enough.
    char message_[core::diag::kMessageBufferSize];
    char line_[core::diag::kLineBufferSize];
    wchar_t path_[kMaxPath];
    char name_[kMaxPath * kUtf8BytesPerUtf16Unit]; // UTF-8 of a MAX_PATH wide name
};

} // namespace omvr::platform::win32
