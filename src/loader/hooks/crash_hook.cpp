// OpenMotherVR-Provenance: clean
#include "loader/hooks/crash_hook.h"

#include "core/diag/crash_state.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "platform/win32/process_info.h"

#include <windows.h>

#include <span>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// The unhandled-exception filter. Only the first crash is recorded: a crash
// while logging must not recurse.
LONG WINAPI on_crash(EXCEPTION_POINTERS* info) noexcept { return context().crash.on_crash(info); }

static_assert(std::is_convertible_v<decltype(&on_crash), LPTOP_LEVEL_EXCEPTION_FILTER>);
static_assert(std::is_same_v<LPTOP_LEVEL_EXCEPTION_FILTER, platform::win32::CrashRecorder::Filter>);

// 22 §14: the mod's state at the crash, on the faulting thread. Only
// atomic loads and plain reads of the manager: no lock, no allocation.
std::size_t render_crash_state(std::span<char> out) noexcept {
    auto& ctx                     = context();
    game::StereoInterface* device = game::stereo::load_device(ctx.manager);
    const core::diag::CrashState state{
        .thread          = ctx.thread_roles.role_of(platform::win32::current_thread_id()),
        .device_attached = device != nullptr,
        .device_state    = static_cast<std::int32_t>(game::stereo::load_state(ctx.manager.si)),
        .stereo_mode     = ctx.manager.mode,
        .interaction     = static_cast<std::uint32_t>(ctx.interactions.interaction()),
        .hiding          = ctx.interactions.hiding(),
        .screen          = static_cast<std::uint32_t>(ctx.play.screen()),
        .cutscene        = ctx.play.cutscene(),
        .vr_turned_off   = ctx.stereo.turned_off()};
    return core::diag::format_crash_state(out, state);
}

} // namespace

void install_crash_hook() noexcept { context().crash.install(&on_crash); }

void reassert_crash_hook() noexcept { context().crash.reassert(&on_crash); }

void enable_crash_safe_log() noexcept {
    auto& ctx = context();
    if (!ctx.settings.current().controls.crash_safe_log) return;
    ctx.log_out.enable();
    ctx.crash.enable_crash_safe(&render_crash_state, ctx.log_flusher);
}

void start_log_flusher() noexcept {
    auto& ctx = context();
    if (ctx.log_out.enabled()) ctx.log_flusher.start();
}

} // namespace omvr::loader::hooks
