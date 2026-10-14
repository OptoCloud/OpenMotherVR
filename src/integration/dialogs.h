#pragma once

#include <string>

// Message boxes reported while the DLL attaches. DllMain runs under the
// loader lock, where showing UI can deadlock, so they are queued and shown
// on a thread of their own, which the loader starts only after it has
// released the lock.
namespace mothervr::integration {

// Queues a box for show_posted_message_boxes(). `text` is UTF-8, `type` is
// MB_* flags. `then`, if given, runs on the dialog thread once the box is
// closed.
void post_message_box(std::string text, unsigned type, void (*then)() = nullptr) noexcept;

// Starts the thread that shows the queued boxes, one after another. Call
// once, at the end of DLL_PROCESS_ATTACH; no-op if nothing was queued.
void show_posted_message_boxes() noexcept;

// Blocks until every queued box is closed. The game's
// D3D11CreateDeviceAndSwapChain waits here, so the game does not start
// rendering under a box, as with the original, which showed them in DllMain.
// Never call it under the loader lock.
void wait_for_message_boxes() noexcept;

} // namespace mothervr::integration
