#include "loader/context.h"

#include "core/ui/ui_contract.h"
#include "loader/resource_ids.h"
#include "platform/openvr/openvr_runtime.h"
#include "platform/openxr/openxr_runtime.h"
#include "platform/win32/process_info.h"

#include <memory>

namespace omvr::loader {

namespace {

Context* g_context = nullptr;

} // namespace

Context::Context(void* self) noexcept
    : module(self),
      log(clock, log_sink, platform::win32::current_process_id()),
      settings(files),
      dialogs(core::ui::kCaption),
      archive_files(self, OMVR_IDR_OPENVR_API, OMVR_IDR_UI_PATCH),
      ui_archive(archive_files, dialogs),
      display_reset(console),
      schemes(glyphs),
      pad(input_manager, glyphs),
      controls(pad, schemes),
      settings_push(script_vars, display_reset, schemes),
      commands(display_reset, console),
      xr(dialogs),
      openxr_probe(xr),
      runtimes(openvr_probe, openxr_probe),
      virtual_screen(xr),
      headset_audio(dialogs),
      modes(manager),
      update(pad, input_manager, follow, scene, modes),
      stereo(manager, update, dialogs),
      head_clip(clock),
      stereo_aa(stereo),
      reflections(manager, keyboard),
      screen_policy(virtual_screen, keyboard, stereo),
      window(manager) {}

Context& create_context(void* module) noexcept {
    // Leaked on purpose: nothing of the mod runs at process exit (no
    // destructors under the loader lock).
    static Context& ctx = *new Context(module);
    g_context = &ctx;

    // R: every area: the objects below are reached through these until their
    // callers take them at construction.
    core::diag::set_process_log(&ctx.log);
    core::config::set_process_settings(&ctx.settings);
    game::set_process_addresses(&ctx.addresses);
    core::vr::set_process_detector(&ctx.runtimes);
    core::stereo::set_process_stereo(&ctx.stereo);

    // The debug setting the logger reports may change while the game runs.
    ctx.log.set_debug_flag(&ctx.settings.current().debug.debug);

    // The runtime the stereo device drives, made when the game creates its
    // stereo device.
    ctx.stereo.set_runtime_factory(
        [c = &ctx](core::vr::detect_result::Chosen chosen) -> std::unique_ptr<core::vr::ports::VrRuntime> {
            if (chosen == core::vr::detect_result::Chosen::OpenVR) {
                return platform::openvr::make_openvr_runtime({c->archive_files, c->controls, c->dialogs});
            }
            return platform::openxr::make_openxr_runtime({c->xr, c->controls, c->headset_audio, c->console});
        });
    return ctx;
}

Context& context() noexcept { return *g_context; }

} // namespace omvr::loader
