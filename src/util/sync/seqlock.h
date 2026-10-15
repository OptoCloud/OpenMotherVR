// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <immintrin.h>

namespace omvr::util {

// A value handed from one writer thread to readers on other threads without
// tearing: a seqlock whose payload is stored as relaxed atomic words, so a
// reader that races the writer retries instead of reading a half-written
// pose. The writer never waits; a reader spins only while a store is in
// progress (a few dozen stores, nanoseconds).
template <class T>
class SeqLock {
    static_assert(std::is_trivially_copyable_v<T>);
    using Word                          = std::uint32_t;
    static constexpr std::size_t kWords = (sizeof(T) + sizeof(Word) - 1) / sizeof(Word);

public:
    SeqLock() noexcept { store(T{}); }
    SeqLock(const SeqLock&)            = delete;
    SeqLock& operator=(const SeqLock&) = delete;

    // One writer at a time. The sequence is odd while a store is in
    // progress. Relaxed word stores are enough: the release fence after the
    // odd sequence keeps them after it, and the release store of the even
    // sequence publishes them.
    void store(const T& value) noexcept {
        std::array<Word, kWords> buf{};
        std::memcpy(buf.data(), &value, sizeof(T));
        const Word s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < kWords; ++i) words_[i].store(buf[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);
    }

    // Any thread. The acquire fence keeps the word loads before the second
    // sequence load; an unchanged even sequence means no store overlapped.
    [[nodiscard]] T load() const noexcept {
        std::array<Word, kWords> buf{};
        for (;;) {
            const Word s = seq_.load(std::memory_order_acquire);
            if (writing(s)) {
                // The x86 spin-wait hint (what YieldProcessor expands to).
                _mm_pause();
                continue;
            }
            for (std::size_t i = 0; i < kWords; ++i) buf[i] = words_[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq_.load(std::memory_order_relaxed) == s) break;
        }
        T out;
        std::memcpy(&out, buf.data(), sizeof(T));
        return out;
    }

private:
    static constexpr bool writing(Word sequence) noexcept { return (sequence & 1u) != 0; }

    std::atomic<Word> seq_{0};
    std::array<std::atomic<Word>, kWords> words_{};
};

} // namespace omvr::util
