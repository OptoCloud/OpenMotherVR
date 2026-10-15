#include "dialogs.h"

#include "diag/log.h"

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include <windows.h>

namespace mothervr::integration {

namespace {

struct Posted {
    std::string text;
    unsigned type;
    void (*then)();
    const char* kind; // static string or null
};

// Filled under the loader lock and read by the dialog thread only after
// CreateThread, so it needs no lock of its own.
std::vector<Posted> g_posted;
std::atomic<HANDLE> g_thread{nullptr};

DWORD WINAPI show_all(void*) {
    for (const Posted& p : g_posted) {
        // MB_SETFOREGROUND: the thread has no window of its own, so without
        // it the box can open behind the game's.
        const int pressed = diag::message_box(p.text, p.type | MB_SETFOREGROUND, p.kind);
        // 0: the box never appeared (logged); what follows a box the
        // player closed must wait for one that is.
        if (pressed != 0 && p.then) p.then();
    }
    return 0;
}

} // namespace

void post_message_box(std::string text, unsigned type, void (*then)(), const char* kind) noexcept {
    try {
        g_posted.push_back({std::move(text), type, then, kind});
    } catch (...) {
        diag::log_line("dialogs: cannot queue a message box");
    }
}

void show_posted_message_boxes() noexcept {
    if (g_posted.empty() || g_thread.load()) return;
    HANDLE h = ::CreateThread(nullptr, 0, &show_all, nullptr, 0, nullptr);
    if (!h) {
        const DWORD error = ::GetLastError();
        diag::logf("dialogs: CreateThread failed err=0x{:08X}", error);
        // The player will not see these: keep their content in the log.
        for (const Posted& p : g_posted)
            diag::logf("dialogs: message ({}) not shown, err=0x{:08X}: {}", p.kind ? p.kind : "unnamed", error,
                       p.text);
        return;
    }
    g_thread.store(h);
}

void wait_for_message_boxes() noexcept {
    if (HANDLE h = g_thread.load()) ::WaitForSingleObject(h, INFINITE);
}

void show_message_box_async(std::string text, unsigned type, const char* kind) noexcept {
    try {
        auto* box = new Posted{std::move(text), type, nullptr, kind};
        auto show = [](void* arg) -> DWORD {
            std::unique_ptr<Posted> p{static_cast<Posted*>(arg)};
            diag::message_box(p->text, p->type | MB_SETFOREGROUND, p->kind);
            return 0;
        };
        if (HANDLE h = ::CreateThread(nullptr, 0, show, box, 0, nullptr)) {
            ::CloseHandle(h);
        } else {
            const DWORD error = ::GetLastError();
            diag::logf("dialogs: message ({}) not shown, CreateThread failed err=0x{:08X}: {}",
                       box->kind ? box->kind : "unnamed", error, box->text);
            delete box;
        }
    } catch (...) {
        diag::log_line("dialogs: cannot show a message box");
    }
}

} // namespace mothervr::integration
