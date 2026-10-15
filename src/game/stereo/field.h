#pragma once

#include <cstddef>
#include <cstdint>

// Fields of AI.exe objects the game has no symbols for, by byte offset. The
// casts from an object's address to a field are here and nowhere else in
// game/stereo (STYLEGUIDE 4: one reinterpret_cast helper per kind).
namespace omvr::game::stereo {

// The field of type T at `offset` bytes into the game object at `base`.
template <class T>
T& field(void* base, std::ptrdiff_t offset) noexcept {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + offset);
}

template <class T>
const T& field(const void* base, std::ptrdiff_t offset) noexcept {
    return *reinterpret_cast<const T*>(static_cast<const std::uint8_t*>(base) + offset);
}

// The pointer stored at `offset` bytes into the game object at `base`.
template <class T = void>
T* field_ptr(void* base, std::ptrdiff_t offset) noexcept {
    return field<T*>(base, offset);
}

} // namespace omvr::game::stereo
