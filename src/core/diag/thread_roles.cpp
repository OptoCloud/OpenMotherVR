// OpenMotherVR-Provenance: clean
#include "core/diag/thread_roles.h"

namespace omvr::core::diag {

std::string_view thread_name(ThreadRole role) noexcept {
    switch (role) {
        case ThreadRole::Main: return "main";
        case ThreadRole::Render: return "render";
        case ThreadRole::Job: return "job";
        case ThreadRole::Mod: return "mod";
        case ThreadRole::Other: break;
    }
    return "other";
}

void ThreadRoles::set(ThreadRole role, std::uint32_t id) noexcept {
    switch (role) {
        case ThreadRole::Main: main_.store(id, std::memory_order_relaxed); break;
        case ThreadRole::Render: render_.store(id, std::memory_order_relaxed); break;
        case ThreadRole::Mod: mod_.store(id, std::memory_order_relaxed); break;
        case ThreadRole::Job:
        case ThreadRole::Other: break;
    }
}

ThreadRole ThreadRoles::role_of(std::uint32_t id) const noexcept {
    if (id == 0) return ThreadRole::Other;
    if (id == main_.load(std::memory_order_relaxed)) return ThreadRole::Main;
    if (id == render_.load(std::memory_order_relaxed)) return ThreadRole::Render;
    if (id == mod_.load(std::memory_order_relaxed)) return ThreadRole::Mod;
    return ThreadRole::Other;
}

} // namespace omvr::core::diag
