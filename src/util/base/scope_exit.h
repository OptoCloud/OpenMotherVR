// OpenMotherVR-Provenance: clean
#pragma once

#include <utility>

namespace omvr::util {

// Runs `fn` when the scope ends, unless released first. `fn` must not
// throw: it runs from a destructor.
template <class Fn>
class [[nodiscard]] ScopeExit {
public:
    explicit ScopeExit(Fn fn) noexcept : fn_(std::move(fn)) {}
    ScopeExit(const ScopeExit&)            = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit(ScopeExit&&)                 = delete;
    ScopeExit& operator=(ScopeExit&&)      = delete;
    ~ScopeExit() noexcept {
        if (active_) fn_();
    }

    void release() noexcept { active_ = false; }

private:
    Fn fn_;
    bool active_ = true;
};

} // namespace omvr::util
