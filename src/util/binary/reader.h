#pragma once

#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

namespace omvr::util {

// Bounds-checked reader over a byte span, for the little-endian formats the
// mod parses (the target is x86, so a value is copied as stored). A read
// past the end yields a zero value or an empty span and leaves the reader
// failed for good, so a parser can read a whole header and check ok() once.
class Reader {
public:
    explicit Reader(std::span<const std::byte> data) noexcept : data_(data) {}

    // False once any read ran past the end.
    [[nodiscard]] bool ok() const noexcept { return ok_; }

    // The next sizeof(T) bytes as a T.
    template <class T>
    [[nodiscard]] T get() noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        T v{};
        if (!take(sizeof(T))) return v;
        std::memcpy(&v, data_.data() + pos_ - sizeof(T), sizeof(T));
        return v;
    }

    // The next `n` bytes, viewing the input.
    [[nodiscard]] std::span<const std::byte> bytes(std::size_t n) noexcept {
        if (!take(n)) return {};
        return data_.subspan(pos_ - n, n);
    }

private:
    bool take(std::size_t n) noexcept {
        if (!ok_ || data_.size() - pos_ < n) return ok_ = false;
        pos_ += n;
        return true;
    }

    std::span<const std::byte> data_;
    std::size_t                pos_ = 0;
    bool                       ok_  = true;
};

} // namespace omvr::util
