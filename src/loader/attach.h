#pragma once

namespace omvr::loader {

struct Context;

// What has to be in place before game code runs, at DLL_PROCESS_ATTACH and
// under the loader lock, in this order: the session line, the settings
// (later steps branch on them), the exe check, the address table,
// MOTHER.PAK and the code and vtable patches (install). MOTHER.PAK stays
// here: the game opens its UI archive during startup, through the path
// patched here. The messages it queues are shown by the dialog thread the
// caller starts once the loader releases its lock.
void attach(Context& ctx);

} // namespace omvr::loader
