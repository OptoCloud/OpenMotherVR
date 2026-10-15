// OpenMotherVR-Provenance: clean
#include "loader/hooks/file_hooks.h"

#include "loader/context.h"
#include "platform/win32/patching/import_slot.h"
#include "platform/win32/patching/memory.h"

#include <atomic>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <windows.h>

namespace omvr::loader::hooks {

namespace {

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFileWFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

// The functions the slots held before; set once, before the swap.
std::atomic<CreateFileAFn> g_create_file_a{nullptr};
std::atomic<CreateFileWFn> g_create_file_w{nullptr};

// Level paths are ASCII; a longer path is not one of them.
constexpr std::size_t kPathChars = 512;

HANDLE WINAPI create_file_a(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                            DWORD disposition, DWORD flags, HANDLE templ) noexcept {
    if (name) context().play.on_file_open(name);
    return g_create_file_a.load(std::memory_order_relaxed)(name, access, share, security, disposition, flags,
                                                           templ);
}

HANDLE WINAPI create_file_w(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                            DWORD disposition, DWORD flags, HANDLE templ) noexcept {
    if (name) {
        char path[kPathChars];
        std::size_t n = 0;
        for (; n < kPathChars && name[n] != L'\0'; ++n)
            path[n] = name[n] < 0x80 ? static_cast<char>(name[n]) : '?';
        if (n < kPathChars) context().play.on_file_open(std::string_view{path, n});
    }
    return g_create_file_w.load(std::memory_order_relaxed)(name, access, share, security, disposition, flags,
                                                           templ);
}

static_assert(std::is_convertible_v<decltype(&create_file_a), CreateFileAFn>);
static_assert(std::is_convertible_v<decltype(&create_file_w), CreateFileWFn>);

template <class Fn>
void take(void* game_module, const char* function, std::atomic<Fn>& original, Fn replacement) noexcept {
    auto& log   = context().log;
    void** slot = platform::win32::import_slot(game_module, "kernel32.dll", function);
    if (!slot) {
        core::diag::ports::logf(log, "files: AI.exe does not import {}, not watched", function);
        return;
    }
    original.store(reinterpret_cast<Fn>(*slot), std::memory_order_relaxed);
    if (!platform::win32::replace_pointer(slot, reinterpret_cast<void*>(replacement), function, log))
        original.store(nullptr, std::memory_order_relaxed);
}

} // namespace

void watch_game_file_opens(void* game_module) noexcept {
    platform::win32::pin_this_module(context().log);
    take<CreateFileAFn>(game_module, "CreateFileA", g_create_file_a, &create_file_a);
    take<CreateFileWFn>(game_module, "CreateFileW", g_create_file_w, &create_file_w);
    if (g_create_file_a.load() || g_create_file_w.load())
        context().log.line("files: watching the game's level loads");
}

} // namespace omvr::loader::hooks
