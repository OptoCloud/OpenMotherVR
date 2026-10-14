#pragma once

#include <windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace mothervr::runtime {

// A value handed from one writer thread to readers on other threads without
// tearing: a seqlock whose payload is stored as relaxed atomic words, so a
// reader that races the writer retries instead of reading a half-written
// pose. The writer never waits; a reader spins only while a store is in
// progress (a few dozen stores, nanoseconds).
template <class T>
class SeqLock {
    static_assert(std::is_trivially_copyable_v<T>);
    static constexpr std::size_t kWords = (sizeof(T) + 3) / 4;

public:
    SeqLock() noexcept { store(T{}); }
    SeqLock(const SeqLock&) = delete;
    SeqLock& operator=(const SeqLock&) = delete;

    // One writer at a time.
    void store(const T& value) noexcept {
        std::uint32_t buf[kWords]{};
        std::memcpy(buf, &value, sizeof(T));
        const std::uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < kWords; ++i) words_[i].store(buf[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);
    }

    T load() const noexcept {
        std::uint32_t buf[kWords]{};
        for (;;) {
            const std::uint32_t s = seq_.load(std::memory_order_acquire);
            if (s & 1) {
                YieldProcessor();
                continue;
            }
            for (std::size_t i = 0; i < kWords; ++i) buf[i] = words_[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_relaxed) == s) break;
        }
        T out;
        std::memcpy(&out, buf, sizeof(T));
        return out;
    }

private:
    std::atomic<std::uint32_t>                 seq_{0};
    std::array<std::atomic<std::uint32_t>, kWords> words_{};
};

} // namespace mothervr::runtime
