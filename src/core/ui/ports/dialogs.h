#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace omvr::core::ui::ports {

// The icon and buttons of a message to the player (every message has OK).
enum class DialogKind { Information, Warning, Error };

// Message boxes to the player. `label` names the message in the log (a
// tester searches the log for it; spec 11 section 2.6); empty for none. It
// must refer to a string that lives for the whole process (a literal).
class Dialogs {
public:
    // Queues a box reported while the DLL attaches, shown once the loader
    // has released its lock. `then`, if given, runs on the dialog thread
    // once the player has closed the box; not if the box could not be shown.
    virtual void post(std::string text, DialogKind kind, std::move_only_function<void()> then,
                      std::string_view label) noexcept = 0;
    // Shows a box on a thread of its own and returns at once, for reports
    // made while the game runs (the render thread must not wait on the
    // player).
    virtual void show_async(std::string text, DialogKind kind, std::string_view label) noexcept = 0;
    // Shows a box and waits until the player closes it. Returns the button
    // pressed, or 0 if the box could not be shown (logged).
    virtual int show_now(std::string_view text, DialogKind kind, std::string_view label) noexcept = 0;
    // Blocks until every posted box is closed. Never under the loader lock.
    virtual void wait_posted() noexcept = 0;

protected:
    ~Dialogs() = default;
};

} // namespace omvr::core::ui::ports
