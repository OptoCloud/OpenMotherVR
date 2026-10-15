#pragma once

#include "core/diag/ports/log.h"
#include "core/diag/tuning.h"
#include "platform/win32/constants.h"

#include <atomic>
#include <cstddef>
#include <format>
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

    // Installs `ours` as the unhandled-exception filter. Call once, at attach.
    void install(Filter ours) noexcept;

    // Puts `ours` back on top if something installed another one after
    // attach (the game or its C runtime), which would otherwise silently drop
    // the crash record. The filter found there is kept and runs after the
    // record. Logs once per change. Cheap (one system call), but not for the
    // render thread's every frame: call it at points the game passes now and
    // then, such as a menu opening or the first frame.
    void reassert(Filter ours) noexcept;

    // The filter's work: the record (first crash only), then the chain. On
    // the faulting thread, in whatever state the process is.
    long on_crash(_EXCEPTION_POINTERS* info) noexcept;

private:
    template <class... Args>
    void record(std::format_string<Args...> fmt, Args&&... args) noexcept;
    void write_record(const _EXCEPTION_POINTERS& info) noexcept;
    void write_stack(const _EXCEPTION_POINTERS& info) noexcept;
    long chain(_EXCEPTION_POINTERS* info) noexcept;

    core::diag::ports::Log& log_;
    Filter previous_ = nullptr;
    std::atomic<bool> logged_{false};
    // A filter the game (or its C runtime) installed over ours after attach.
    // Ours is put back on top and this one runs after the record, so neither
    // the record nor the game's own handling is lost.
    std::atomic<Filter> game_filter_{nullptr};
    // The thread currently inside game_filter_: if that filter chains to the
    // one it replaced (ours), the nested call goes on to previous_ instead of
    // looping back into it.
    std::atomic<unsigned long> chaining_thread_{0};

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
