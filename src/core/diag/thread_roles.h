// OpenMotherVR-Provenance: clean
#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

// Which of the process's threads is which (11 §1.4): the log's flush policy
// (22 §14) and the crash event name the thread by its role.
namespace omvr::core::diag {

// The threads 11 §1.4 names. Main is the game thread, which also runs
// attach (the loader thread).
enum class ThreadRole : std::uint8_t { Main, Render, Job, Mod, Other };

// The role's name as 11 §1.4 writes it: main, render, job, mod, other.
std::string_view thread_name(ThreadRole role) noexcept;

// The ids of the threads whose role is known. Each is set once by the
// thread itself (the game thread at attach, the render thread at its first
// Present, the mod's own thread when it starts) and read from any thread,
// a crashing one included: atomics, no lock, no allocation.
class ThreadRoles final {
public:
    // `id` is the calling thread's system id. Main, Render and Mod only;
    // other roles are not recorded.
    void set(ThreadRole role, std::uint32_t id) noexcept;

    // The role of thread `id`; Other for a thread not recorded (job
    // threads, runtime threads, threads of other modules).
    ThreadRole role_of(std::uint32_t id) const noexcept;

private:
    // 0 is never a valid thread id on Windows: it means "not known yet".
    // Relaxed: each is a single independent word.
    std::atomic<std::uint32_t> main_{0};
    std::atomic<std::uint32_t> render_{0};
    std::atomic<std::uint32_t> mod_{0};
};

} // namespace omvr::core::diag
