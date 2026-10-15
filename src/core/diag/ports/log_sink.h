// OpenMotherVR-Provenance: clean
#pragma once

#include <span>

namespace omvr::core::diag::ports {

// Where finished log lines go (the log file). Any thread.
class LogSink {
public:
    // Whether lines can be written at all.
    virtual bool ready() const noexcept = 0;
    // Writes one finished line, line ending included, in one piece.
    virtual void write(std::span<const char> line) noexcept = 0;

protected:
    ~LogSink() = default;
};

} // namespace omvr::core::diag::ports
