// OpenMotherVR-Provenance: clean
#pragma once

#include <utility>

namespace omvr::platform::win32 {

// Owns one reference to a COM object and releases it when it goes out of
// scope. T is any interface with AddRef and Release; no Windows header is
// needed here, so adapter headers may hold one.
template <class T>
class ComPtr {
public:
    ComPtr() noexcept = default;
    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) ptr_->AddRef();
    }
    ComPtr(ComPtr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}
    ComPtr& operator=(ComPtr other) noexcept {
        std::swap(ptr_, other.ptr_);
        return *this;
    }
    ~ComPtr() { reset(); }

    // Takes over the reference `p` already holds (an out parameter filled
    // by a call that succeeded).
    static ComPtr adopt(T* p) noexcept {
        ComPtr owner;
        owner.ptr_ = p;
        return owner;
    }

    // Takes a reference of its own to `p` (AddRef), which the caller keeps.
    static ComPtr share(T* p) noexcept {
        if (p) p->AddRef();
        return adopt(p);
    }

    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    void reset() noexcept {
        if (T* p = std::exchange(ptr_, nullptr)) p->Release();
    }

    // Releases what is held and returns the slot for an out parameter.
    T** put() noexcept {
        reset();
        return &ptr_;
    }

private:
    T* ptr_ = nullptr;
};

// Releases `p` and clears it: for raw COM pointers kept where a ComPtr
// cannot be (the game's layout, arrays a D3D11 call fills).
template <class T>
void release_one(T*& p) noexcept {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

} // namespace omvr::platform::win32
