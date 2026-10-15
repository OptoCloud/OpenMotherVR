#include "platform/win32/command_line.h"

#include <windows.h>
#include <shellapi.h>

namespace omvr::platform::win32 {

std::wstring_view process_command_line() noexcept {
    const wchar_t* line = ::GetCommandLineW();
    return line ? line : L"";
}

std::vector<std::wstring> process_arguments() {
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) return {};
    std::vector<std::wstring> args;
    try {
        args.assign(argv, argv + argc);
    } catch (...) {
        ::LocalFree(argv);
        throw;
    }
    ::LocalFree(argv);
    return args;
}

} // namespace omvr::platform::win32
