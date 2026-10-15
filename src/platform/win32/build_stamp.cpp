// From: inline code in FUN_10002250 @ 0x10002250 (DllMain) around
//       offsets +0x70..+0x150. The original used IMAGEHLP's MapAndLoad to
//       parse the PE on disk; that requires a working DllPath search path,
//       which is fragile. We just read the headers directly from the
//       already-loaded module: same information, no file I/O.

#include "platform/win32/build_stamp.h"

#include "platform/win32/constants.h"
#include "platform/win32/patching/pe_image.h"

#include <windows.h>

namespace omvr::platform::win32 {

std::int64_t this_module_build_filetime() {
    HMODULE self = nullptr;
    ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                         | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&this_module_build_filetime),
                         &self);
    auto ts = loaded_image_timestamp(self);
    if (!ts) return 0;

    return static_cast<std::int64_t>(*ts) * kFileTimeTicksPerSecond + kFileTimeUnixEpoch;
}

} // namespace omvr::platform::win32
