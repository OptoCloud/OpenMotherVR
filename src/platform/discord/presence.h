// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace omvr::platform::discord {

// What the player's Discord profile shows under the application's name.
struct Activity {
    std::string details;         // first line
    std::string state;           // second line
    std::int64_t start_unix = 0; // "elapsed" counts from here; 0: none
    std::string large_text;      // the image's hover text; empty: kLargeText

    bool operator==(const Activity&) const = default;
};

// Discord Rich Presence over the Discord client's local IPC pipe
// (\\.\pipe\discord-ipc-N): a handshake with the application ID, then
// SET_ACTIVITY whenever the activity changes. Everything runs on one worker
// thread, so the game and render threads never wait on Discord. Without
// Discord running it retries now and then and otherwise does nothing.
class Presence {
public:
    // Polled on the worker every few seconds; must be safe to call from any
    // thread (read atomics only).
    using Source = std::function<Activity()>;

    // Starts the worker. `application_id` is the Discord application's ID;
    // empty does nothing. Call once. The worker is never joined: the mod does
    // no work at process exit, and Discord clears the activity when the pipe
    // closes with the process.
    void start(std::string application_id, Source source, core::diag::ports::Log& log) noexcept;

private:
    void run() noexcept;

    std::string application_id_;
    Source source_;
    core::diag::ports::Log* log_ = nullptr;
    std::atomic<bool> started_{false};
    std::thread worker_;
};

} // namespace omvr::platform::discord
