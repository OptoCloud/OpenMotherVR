#pragma once

#include <cstddef>
#include <cstring>
#include <span>

namespace omvr::util {

// Bounds-checked little-endian reader over a byte span.
class Reader {
public:
    explicit Reader(std::span<const std::byte> data) : data_(data) {}
    bool ok() const noexcept { return ok_; }
    template <class T>
    T get() noexcept {
        T v{};
        if (!take(sizeof(T))) return v;
        std::memcpy(&v, data_.data() + pos_ - sizeof(T), sizeof(T));
        return v;
    }
    std::span<const std::byte> bytes(std::size_t n) noexcept {
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
    std::size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace omvr::util
