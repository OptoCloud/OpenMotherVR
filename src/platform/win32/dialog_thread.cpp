#include "platform/win32/dialog_thread.h"

#include "platform/win32/message_box.h"
#include "platform/win32/unique_handle.h"

#include <memory>
#include <utility>

#include <windows.h>

namespace omvr::platform::win32 {

using core::ui::ports::DialogKind;

namespace {

// A box shown on a thread of its own by show_async.
struct AsyncBox {
    DialogThread* owner;
    std::string   text;
    unsigned      type;
    const char*   kind;
};

unsigned box_type(DialogKind kind) noexcept {
    switch (kind) {
        case DialogKind::Information: return MB_OK | MB_ICONINFORMATION;
        case DialogKind::Warning:     return MB_OK | MB_ICONWARNING;
        case DialogKind::Error:       return MB_OK | MB_ICONERROR;
    }
    return MB_OK;
}

// MB_SETFOREGROUND: the dialog threads have no window of their own, so
// without it a box can open behind the game's.
constexpr unsigned kFromBackgroundThread = MB_SETFOREGROUND;

const char* kind_of(std::string_view label) noexcept { return label.empty() ? nullptr : label.data(); }

const char* name_of(const char* kind) noexcept { return kind ? kind : "unnamed"; }

} // namespace

int DialogThread::show(std::string_view text, unsigned type, const char* kind) noexcept {
    return message_box(log_, text, type, kind, caption_);
}

void DialogThread::log_not_shown(const Posted& p, unsigned long error) noexcept {
    core::diag::ports::logf(log_, "dialogs: message ({}) not shown, err=0x{:08X}: {}", name_of(p.kind), error, p.text);
}

unsigned long __stdcall DialogThread::show_all(void* arg) {
    auto& self = *static_cast<DialogThread*>(arg);
    for (Posted& p : self.posted_) {
        const int pressed = self.show(p.text, p.type | kFromBackgroundThread, p.kind);
        // 0: the box never appeared (logged); what follows a box the
        // player closed must wait for one that is.
        if (pressed != 0 && p.then) p.then();
    }
    return 0;
}

unsigned long __stdcall DialogThread::show_one(void* arg) {
    const std::unique_ptr<AsyncBox> box{static_cast<AsyncBox*>(arg)};
    box->owner->show(box->text, box->type | kFromBackgroundThread, box->kind);
    return 0;
}

void DialogThread::post(std::string text, DialogKind kind, std::move_only_function<void()> then,
                        std::string_view label) noexcept {
    try {
        posted_.push_back({std::move(text), box_type(kind), std::move(then), kind_of(label)});
    } catch (...) {
        log_.line("dialogs: cannot queue a message box");
    }
}

void DialogThread::start() noexcept {
    if (posted_.empty() || thread_.load()) return;
    HANDLE h = ::CreateThread(nullptr, 0, &show_all, this, 0, nullptr);
    if (!h) {
        const DWORD error = ::GetLastError();
        core::diag::ports::logf(log_, "dialogs: CreateThread failed err=0x{:08X}", error);
        // The player will not see these: keep their content in the log.
        for (const Posted& p : posted_) log_not_shown(p, error);
        return;
    }
    thread_.store(h);
}

void DialogThread::wait_posted() noexcept {
    if (HANDLE h = thread_.load()) ::WaitForSingleObject(h, INFINITE);
}

int DialogThread::show_now(std::string_view text, DialogKind kind, std::string_view label) noexcept {
    return show(text, box_type(kind), kind_of(label));
}

void DialogThread::show_async(std::string text, DialogKind kind, std::string_view label) noexcept {
    try {
        auto box = std::make_unique<AsyncBox>(AsyncBox{this, std::move(text), box_type(kind), kind_of(label)});
        // The thread owns the box once it runs; the handle is not needed.
        if (const UniqueHandle thread{::CreateThread(nullptr, 0, &show_one, box.get(), 0, nullptr)}) {
            static_cast<void>(box.release());
            return;
        }
        const DWORD error = ::GetLastError();
        core::diag::ports::logf(log_, "dialogs: message ({}) not shown, CreateThread failed err=0x{:08X}: {}",
                                name_of(box->kind), error, box->text);
    } catch (...) {
        log_.line("dialogs: cannot show a message box");
    }
}

} // namespace omvr::platform::win32
