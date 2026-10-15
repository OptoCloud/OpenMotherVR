#pragma once

#include "core/diag/ports/log.h"
#include "core/ui/ports/dialogs.h"

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Message boxes reported while the DLL attaches. DllMain runs under the
// loader lock, where showing UI can deadlock, so they are queued and shown
// on a thread of their own, which the loader starts only after it has
// released the lock.
namespace omvr::platform::win32 {

class DialogThread final : public core::ui::ports::Dialogs {
public:
    // `caption` is the title of every box; it must live for the process.
    DialogThread(const wchar_t* caption, core::diag::ports::Log& log) noexcept : caption_(caption), log_(log) {}

    void post(std::string text, core::ui::ports::DialogKind kind, std::move_only_function<void()> then,
              std::string_view label) noexcept override;
    void show_async(std::string text, core::ui::ports::DialogKind kind, std::string_view label) noexcept override;
    int show_now(std::string_view text, core::ui::ports::DialogKind kind, std::string_view label) noexcept override;
    // The game's D3D11CreateDeviceAndSwapChain waits here, so the game does
    // not start rendering under a box. Never call it under the loader lock.
    void wait_posted() noexcept override;

    // Starts the thread that shows the posted boxes, one after another. Call
    // once, at the end of DLL_PROCESS_ATTACH; no-op if nothing was posted.
    // If the thread cannot be started, each box's text is logged with the
    // reason, wait_posted does not wait, and no `then` runs.
    void start() noexcept;

private:
    struct Posted {
        std::string text;
        unsigned type;
        std::move_only_function<void()> then;
        const char* kind; // static string or null
    };

    // The thread procedures (LPTHREAD_START_ROUTINE).
    static unsigned long __stdcall show_all(void* self);
    static unsigned long __stdcall show_one(void* box);

    int show(std::string_view text, unsigned type, const char* kind) noexcept;
    // Logs the text of a box the player will not see.
    void log_not_shown(const Posted& p, unsigned long error) noexcept;

    const wchar_t*          caption_;
    core::diag::ports::Log& log_;
    // Filled under the loader lock and read by the dialog thread only after
    // CreateThread, so it needs no lock of its own.
    std::vector<Posted> posted_;
    // The dialog thread (a HANDLE), published for wait_posted on the game's
    // threads; never closed, it is waited on for the process's lifetime.
    std::atomic<void*>  thread_{nullptr};
};

} // namespace omvr::platform::win32
