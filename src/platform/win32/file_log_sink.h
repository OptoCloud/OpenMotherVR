#pragma once

#include "core/diag/ports/log_sink.h"

#include <atomic>
#include <filesystem>
#include <span>

namespace omvr::platform::win32 {

// The log file, opened once and kept: opening it per line cost about 0.1 ms,
// and up to 250 ms during a level load. FILE_APPEND_DATA keeps each write an
// atomic append, so the launcher and game processes can share the file.
class FileLogSink final : public core::diag::ports::LogSink {
public:
    // Opens (or creates) `file` for appending. Without it, lines are dropped.
    void open(const std::filesystem::path& file) noexcept;

    bool ready() const noexcept override;
    void write(std::span<const char> line) noexcept override;

private:
    // HANDLE, published once open; never closed: the log lasts the process.
    std::atomic<void*> file_{nullptr};
};

} // namespace omvr::platform::win32
