// OpenMotherVR-Provenance: clean
#pragma once

#include <utility>

namespace omvr::platform::win32 {

// Owns a kernel handle (a HANDLE) and closes it when it goes out of scope.
// Both null and INVALID_HANDLE_VALUE mean "no handle", since Windows uses
// each as the failure value of different functions (CreateFile gives the
// latter, CreateThread and OpenProcess the former).
class UniqueHandle {
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(void* handle) noexcept : handle_(handle) {}
    UniqueHandle(const UniqueHandle&)            = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) reset(std::exchange(other.handle_, nullptr));
        return *this;
    }
    ~UniqueHandle() { reset(); }

    // Whether a handle is held.
    bool valid() const noexcept;
    explicit operator bool() const noexcept { return valid(); }

    void* get() const noexcept { return handle_; }

    // Closes the handle held, if any, and takes `handle`.
    void reset(void* handle = nullptr) noexcept;

    // Gives the handle up without closing it.
    void* release() noexcept { return std::exchange(handle_, nullptr); }

private:
    void* handle_ = nullptr;
};

// Owns a reference to a loaded module (an HMODULE from LoadLibrary) and
// releases it with FreeLibrary when it goes out of scope.
class UniqueModule {
public:
    UniqueModule() noexcept = default;
    explicit UniqueModule(void* module) noexcept : module_(module) {}
    UniqueModule(const UniqueModule&)            = delete;
    UniqueModule& operator=(const UniqueModule&) = delete;
    UniqueModule(UniqueModule&& other) noexcept : module_(std::exchange(other.module_, nullptr)) {}
    UniqueModule& operator=(UniqueModule&& other) noexcept {
        if (this != &other) reset(std::exchange(other.module_, nullptr));
        return *this;
    }
    ~UniqueModule() { reset(); }

    explicit operator bool() const noexcept { return module_ != nullptr; }
    void* get() const noexcept { return module_; }

    // Takes `module`, then releases the reference held before, if any.
    void reset(void* module = nullptr) noexcept;

    // Gives the reference up without releasing it.
    void* release() noexcept { return std::exchange(module_, nullptr); }

private:
    void* module_ = nullptr;
};

} // namespace omvr::platform::win32
