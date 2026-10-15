#pragma once

#include <string>

// Message boxes reported while the DLL attaches. DllMain runs under the
// loader lock, where showing UI can deadlock, so they are queued and shown
// on a thread of their own, which the loader starts only after it has
// released the lock.
namespace mothervr::integration {

// Queues a box for show_posted_message_boxes(). `text` is UTF-8, `type` is
// MB_* flags. `then`, if given, runs on the dialog thread once the player
// has closed the box; not if the box could not be shown (so a first-launch
// file tied to the welcome is not written for a welcome nobody saw).
// `kind` names the message in the log (see diag::message_box).
void post_message_box(std::string text, unsigned type, void (*then)() = nullptr,
                      const char* kind = nullptr) noexcept;

// Starts the thread that shows the queued boxes, one after another. Call
// once, at the end of DLL_PROCESS_ATTACH; no-op if nothing was queued. If
// the thread cannot be started, each box's text is logged with the reason,
// device creation does not wait, and no `then` runs.
void show_posted_message_boxes() noexcept;

// Blocks until every queued box is closed. The game's
// D3D11CreateDeviceAndSwapChain waits here, so the game does not start
// rendering under a box, as with the original, which showed them in DllMain.
// Never call it under the loader lock.
void wait_for_message_boxes() noexcept;

// Shows a box on a thread of its own and returns at once, for reports made
// while the game runs (the render thread must not wait on the player).
void show_message_box_async(std::string text, unsigned type, const char* kind = nullptr) noexcept;

} // namespace mothervr::integration
