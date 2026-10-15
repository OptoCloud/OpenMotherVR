#include "platform/win32/command_line.h"

#include <windows.h>
#include <shellapi.h>

#include <memory>

namespace omvr::platform::win32 {

namespace {

// CommandLineToArgvW's array is one LocalAlloc block.
struct LocalFreeDeleter {
    void operator()(wchar_t** p) const noexcept { ::LocalFree(p); }
};

} // namespace

std::wstring_view process_command_line() noexcept {
    const wchar_t* line = ::GetCommandLineW();
    return line ? line : L"";
}

std::vector<std::wstring> process_arguments() {
    int argc = 0;
    const std::unique_ptr<wchar_t*, LocalFreeDeleter> argv{::CommandLineToArgvW(::GetCommandLineW(), &argc)};
    if (!argv) return {};
    return std::vector<std::wstring>(argv.get(), argv.get() + argc);
}

} // namespace omvr::platform::win32
