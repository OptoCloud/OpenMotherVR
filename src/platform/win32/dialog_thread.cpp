#include "platform/win32/dialog_thread.h"

#include "core/diag/logger.h" // R: diag (A4): log through an injected Log&
#include "platform/win32/message_box.h"

#include <memory>
#include <utility>

#include <windows.h>

namespace omvr::platform::win32 {

namespace diag = core::diag;
using core::ui::ports::DialogKind;

namespace {

unsigned box_type(DialogKind kind) noexcept {
    switch (kind) {
        case DialogKind::Information: return MB_OK | MB_ICONINFORMATION;
        case DialogKind::Warning:     return MB_OK | MB_ICONWARNING;
        case DialogKind::Error:       return MB_OK | MB_ICONERROR;
    }
    return MB_OK;
}

const char* kind_of(std::string_view label) noexcept { return label.empty() ? nullptr : label.data(); }

} // namespace

int DialogThread::show(std::string_view text, unsigned type, const char* kind) noexcept {
    return message_box(text, type, kind, caption_);
}

unsigned long __stdcall DialogThread::show_all(void* arg) {
    auto& self = *static_cast<DialogThread*>(arg);
    for (Posted& p : self.g_posted) {
        // MB_SETFOREGROUND: the thread has no window of its own, so without
        // it the box can open behind the game's.
        const int pressed = self.show(p.text, p.type | MB_SETFOREGROUND, p.kind);
        // 0: the box never appeared (logged); what follows a box the
        // player closed must wait for one that is.
        if (pressed != 0 && p.then) p.then();
    }
    return 0;
}

void DialogThread::post(std::string text, DialogKind kind, std::move_only_function<void()> then,
                        std::string_view label) noexcept {
    try {
        g_posted.push_back({std::move(text), box_type(kind), std::move(then), kind_of(label)});
    } catch (...) {
        diag::log_line("dialogs: cannot queue a message box");
    }
}

void DialogThread::start() noexcept {
    if (g_posted.empty() || g_thread.load()) return;
    HANDLE h = ::CreateThread(nullptr, 0, &show_all, this, 0, nullptr);
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

void DialogThread::wait_posted() noexcept {
    if (HANDLE h = g_thread.load()) ::WaitForSingleObject(h, INFINITE);
}

int DialogThread::show_now(std::string_view text, DialogKind kind, std::string_view label) noexcept {
    return show(text, box_type(kind), kind_of(label));
}

void DialogThread::show_async(std::string text, DialogKind kind, std::string_view label) noexcept {
    struct Box {
        DialogThread* owner;
        Posted        posted;
    };
    try {
        auto* box = new Box{this, Posted{std::move(text), box_type(kind), nullptr, kind_of(label)}};
        auto show = [](void* arg) -> DWORD {
            std::unique_ptr<Box> p{static_cast<Box*>(arg)};
            p->owner->show(p->posted.text, p->posted.type | MB_SETFOREGROUND, p->posted.kind);
            return 0;
        };
        if (HANDLE h = ::CreateThread(nullptr, 0, show, box, 0, nullptr)) {
            ::CloseHandle(h);
        } else {
            const DWORD error = ::GetLastError();
            diag::logf("dialogs: message ({}) not shown, CreateThread failed err=0x{:08X}: {}",
                       box->posted.kind ? box->posted.kind : "unnamed", error, box->posted.text);
            delete box;
        }
    } catch (...) {
        diag::log_line("dialogs: cannot show a message box");
    }
}

} // namespace omvr::platform::win32
