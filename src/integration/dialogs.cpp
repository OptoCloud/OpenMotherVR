#include "dialogs.h"

#include "diag/log.h"

#include <atomic>
#include <utility>
#include <vector>

#include <windows.h>

namespace mothervr::integration {

namespace {

struct Posted {
    std::string text;
    unsigned type;
    void (*then)();
};

// Filled under the loader lock and read by the dialog thread only after
// CreateThread, so it needs no lock of its own.
std::vector<Posted> g_posted;
std::atomic<HANDLE> g_thread{nullptr};

DWORD WINAPI show_all(void*) {
    for (const Posted& p : g_posted) {
        // MB_SETFOREGROUND: the thread has no window of its own, so without
        // it the box can open behind the game's.
        diag::message_box(p.text, p.type | MB_SETFOREGROUND);
        if (p.then) p.then();
    }
    return 0;
}

} // namespace

void post_message_box(std::string text, unsigned type, void (*then)()) noexcept {
    try {
        g_posted.push_back({std::move(text), type, then});
    } catch (...) {
        diag::log_line("dialogs: cannot queue a message box");
    }
}

void show_posted_message_boxes() noexcept {
    if (g_posted.empty() || g_thread.load()) return;
    HANDLE h = ::CreateThread(nullptr, 0, &show_all, nullptr, 0, nullptr);
    if (!h) {
        diag::logf("dialogs: CreateThread failed err=0x{:08X}", ::GetLastError());
        return;
    }
    g_thread.store(h);
}

void wait_for_message_boxes() noexcept {
    if (HANDLE h = g_thread.load()) ::WaitForSingleObject(h, INFINITE);
}

} // namespace mothervr::integration
