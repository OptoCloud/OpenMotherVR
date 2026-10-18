// OpenMotherVR-Provenance: clean
#include "loader/hooks/file_hooks.h"

#include "core/render/engine_settings.h"
#include "core/render/tuning.h"
#include "game/sites/engine_settings.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"
#include "platform/win32/patching/import_slot.h"
#include "platform/win32/patching/memory.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <windows.h>

namespace omvr::loader::hooks {

namespace {

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFileWFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

constexpr std::string_view kNoAdditionLog =
    "[hook] engine settings: override on but no addition is on; the game reads its own file";
constexpr std::string_view kArmedLog =
    "[hook] engine settings: on; the game's open of \"{}\" goes to a copy with {} addition(s)";
constexpr std::string_view kNoSlotLog =
    "[hook] engine settings: off; AI.exe's CreateFileA import could not be taken, the game reads its own "
    "file";
constexpr std::string_view kCopyOpenFailedLog =
    "[hook] engine settings: cannot open the copy (err=0x{:08X}); the game reads its own file";

// The functions the slots held before; set once, before the swap.
std::atomic<CreateFileAFn> g_create_file_a{nullptr};
std::atomic<CreateFileWFn> g_create_file_w{nullptr};
std::atomic<bool> g_watch_levels{false};

// Level paths are ASCII; a longer path is not one of them.
constexpr std::size_t kPathChars = 512;

// The engine settings redirect (24 §6.1), armed at attach. The copy is built
// once, at the game's first open of its file.
struct EngineSettingsRedirect {
    std::string_view game_path; // AI.exe's own string
    std::filesystem::path player_file;
    std::filesystem::path copy;
    std::vector<core::render::EngineOption> options;
    std::once_flag built;
    bool use_copy = false; // written in the once call, read after it
};

// Leaked like the context: nothing of the mod runs at process exit.
std::atomic<EngineSettingsRedirect*> g_engine_settings{nullptr};

// The copy the game should open instead of `name`, or null. The game opens
// its file to read it (fopen "rb"); any other open is left alone.
const wchar_t* engine_settings_copy_for(std::string_view name, DWORD access, DWORD disposition) noexcept {
    EngineSettingsRedirect* r = g_engine_settings.load(std::memory_order_acquire);
    constexpr DWORD kWrites   = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE;
    if (!r || disposition != OPEN_EXISTING || (access & kWrites) != 0 ||
        !core::render::names_engine_settings(name, r->game_path)) {
        return nullptr;
    }
    return guarded("engine settings", static_cast<const wchar_t*>(nullptr), [&]() -> const wchar_t* {
        std::call_once(r->built, [&] {
            auto& ctx   = context();
            r->use_copy = core::render::build_engine_settings_copy(ctx.files, ctx.log, r->player_file,
                                                                   r->copy, r->options);
        });
        return r->use_copy ? r->copy.c_str() : nullptr;
    });
}

// Opens the copy for the game with its own arguments; on failure the game
// opens its own file.
HANDLE open_copy(const wchar_t* copy, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                 DWORD disposition, DWORD flags, HANDLE templ) noexcept {
    HANDLE h = CreateFileW(copy, access, share, security, disposition, flags, templ);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        guarded("engine settings",
                [&] { core::diag::ports::logf(context().log, kCopyOpenFailedLog, error); });
        SetLastError(error);
    }
    return h;
}

// What the hooks do with the game's open of `name`: note a level load, and
// open the engine settings copy in place of the game's file. The copy's
// handle, or INVALID_HANDLE_VALUE when the game's own open goes ahead.
HANDLE try_game_open(std::string_view name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                     DWORD disposition, DWORD flags, HANDLE templ) noexcept {
    if (g_watch_levels.load(std::memory_order_relaxed)) context().play.on_file_open(name);
    const wchar_t* copy = engine_settings_copy_for(name, access, disposition);
    return copy ? open_copy(copy, access, share, security, disposition, flags, templ) : INVALID_HANDLE_VALUE;
}

HANDLE WINAPI create_file_a(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                            DWORD disposition, DWORD flags, HANDLE templ) noexcept {
    if (name) {
        HANDLE h = try_game_open(name, access, share, security, disposition, flags, templ);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    return g_create_file_a.load(std::memory_order_relaxed)(name, access, share, security, disposition, flags,
                                                           templ);
}

HANDLE WINAPI create_file_w(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                            DWORD disposition, DWORD flags, HANDLE templ) noexcept {
    if (name) {
        char path[kPathChars];
        std::size_t n = 0;
        for (; n < kPathChars && name[n] != L'\0'; ++n)
            path[n] = name[n] < 0x80 ? static_cast<char>(name[n]) : '?';
        if (n < kPathChars) {
            HANDLE h = try_game_open({path, n}, access, share, security, disposition, flags, templ);
            if (h != INVALID_HANDLE_VALUE) return h;
        }
    }
    return g_create_file_w.load(std::memory_order_relaxed)(name, access, share, security, disposition, flags,
                                                           templ);
}

static_assert(std::is_convertible_v<decltype(&create_file_a), CreateFileAFn>);
static_assert(std::is_convertible_v<decltype(&create_file_w), CreateFileWFn>);

template <class Fn>
void take(void* game_module, const char* function, std::atomic<Fn>& original, Fn replacement) noexcept {
    auto& log   = context().log;
    void** slot = platform::win32::import_slot(game_module, "kernel32.dll", function);
    if (!slot) {
        core::diag::ports::logf(log, "files: AI.exe does not import {}, not watched", function);
        return;
    }
    original.store(reinterpret_cast<Fn>(*slot), std::memory_order_relaxed);
    if (!platform::win32::replace_pointer(slot, reinterpret_cast<void*>(replacement), function, log))
        original.store(nullptr, std::memory_order_relaxed);
}

} // namespace

bool prepare_engine_settings(const game::GameImage& g, const std::filesystem::path& game_dir) noexcept {
    auto& ctx                                     = context();
    const core::config::GraphicsSettings graphics = ctx.settings.current().graphics;
    if (!graphics.engine_settings_override) return false;
    return guarded("engine settings", false, [&] {
        auto options = core::render::engine_options(graphics);
        if (options.empty()) {
            ctx.log.line(kNoAdditionLog);
            return false;
        }
        const std::string_view path = game::sites::engine_settings_path(g, ctx.log);
        if (path.empty()) return false;
        auto* r        = new EngineSettingsRedirect{};
        r->game_path   = path;
        r->player_file = game_dir / std::filesystem::path(std::string(path));
        r->copy        = game_dir / core::render::kEngineSettingsCopyName;
        r->options     = std::move(options);
        core::diag::ports::logf(ctx.log, kArmedLog, path, r->options.size());
        g_engine_settings.store(r, std::memory_order_release);
        return true;
    });
}

void watch_game_file_opens(void* game_module, bool levels) noexcept {
    g_watch_levels.store(levels, std::memory_order_relaxed);
    platform::win32::pin_this_module(context().log);
    take<CreateFileAFn>(game_module, "CreateFileA", g_create_file_a, &create_file_a);
    take<CreateFileWFn>(game_module, "CreateFileW", g_create_file_w, &create_file_w);
    if (levels && (g_create_file_a.load() || g_create_file_w.load()))
        context().log.line("files: watching the game's level loads");
    // The game's fopen reaches CreateFileA (24 §1.9).
    if (g_engine_settings.load(std::memory_order_relaxed) && !g_create_file_a.load()) {
        context().log.line(kNoSlotLog);
    }
}

} // namespace omvr::loader::hooks
