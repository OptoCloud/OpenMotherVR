// OpenMotherVR-Provenance: clean
#pragma once

#include "game/code_address.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// Fields of AI.exe objects the game has no symbols for, by byte offset. The
// casts from an object's address to a field are here and nowhere else in
// game/ (STYLEGUIDE 4: one reinterpret_cast helper per kind).
namespace omvr::game {

// The bytes at `offset` into the game object at `object`.
inline const std::uint8_t* bytes_at(const void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<const std::uint8_t*>(object) + offset;
}
inline std::uint8_t* bytes_at(void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<std::uint8_t*>(object) + offset;
}

// The field of type T at `offset` bytes into the game object at `base`, for
// a field the game keeps aligned for its type.
template <class T>
T& field(void* base, std::ptrdiff_t offset) noexcept {
    return *reinterpret_cast<T*>(bytes_at(base, offset));
}

template <class T>
const T& field(const void* base, std::ptrdiff_t offset) noexcept {
    return *reinterpret_cast<const T*>(bytes_at(base, offset));
}

// The pointer stored in the aligned field at `offset` bytes into `base`.
template <class T = void>
T* field_ptr(void* base, std::ptrdiff_t offset) noexcept {
    return field<T*>(base, offset);
}

// The value of type T at `offset` bytes into `object`, copied out: for
// fields whose alignment the mod does not rely on.
template <class T>
T read_field(const void* object, std::ptrdiff_t offset) noexcept {
    T v;
    std::memcpy(&v, bytes_at(object, offset), sizeof(T));
    return v;
}

// Copies `v` into the field at `offset` bytes into `object`.
template <class T>
void write_field(void* object, std::ptrdiff_t offset, const T& v) noexcept {
    std::memcpy(bytes_at(object, offset), &v, sizeof(T));
}

// The pointer stored at `offset` bytes into `object`, copied out.
inline void* pointer_field(const void* object, std::ptrdiff_t offset) noexcept {
    return CodeAddress{read_field<std::uint32_t>(object, offset)};
}

} // namespace omvr::game
