// OpenMotherVR-Provenance: clean
#include "loader/context.h"

#include "core/ui/ui_contract.h"
#include "loader/resource_ids.h"
#include "platform/openvr/openvr_runtime.h"
#include "platform/openxr/openxr_runtime.h"
#include "platform/win32/process_info.h"

#include <memory>

namespace omvr::loader {

namespace {

// Set once, in DLL_PROCESS_ATTACH, before any hook can run: the loader lock
// orders it before every thread that calls context().
Context* the_context = nullptr;

// The runtime the stereo device drives, made when the game creates its
// stereo device.
std::unique_ptr<core::vr::ports::VrRuntime> make_runtime(Context& c, core::vr::DetectResult::Chosen chosen) {
    if (chosen == core::vr::DetectResult::Chosen::OpenVR) {
        return platform::openvr::make_openvr_runtime({c.log, c.clock, c.failure, c.archive_files, c.controls,
                                                      c.dialogs,
                                                      c.runtimes.last_result().openvr_runtime_path});
    }
    return platform::openxr::make_openxr_runtime({c.log, c.xr, c.controls, c.headset_audio, c.console});
}

} // namespace

Context::Context(void* self) noexcept
    : module(self),
      log(clock, log_sink, platform::win32::current_process_id()),
      crash(log),
      settings(files, log),
      dialogs(core::ui::kCaption, log),
      archive_files(self, OMVR_IDR_OPENVR_API, OMVR_IDR_UI_PATCH, log),
      ui_archive(archive_files, dialogs, log),
      runtime_state(runtimes, stereo, launch),
      display_reset(console, runtime_state, log),
      schemes(glyphs),
      pad(glyphs, log),
      controls(pad, schemes, settings.current().input),
      interactions(log),
      settings_push({script_vars, display_reset, schemes, settings.current(), runtime_state, log}),
      commands(settings, display_reset, console, log),
      xr(log, clock, dialogs, failure),
      openvr_probe(log),
      openxr_probe(xr),
      runtimes(log, settings.current(), launch, openvr_probe, openxr_probe),
      virtual_screen(xr),
      headset_audio(dialogs, log, addresses),
      modes(log),
      update(settings.current(), scene),
      stereo(manager, update, dialogs, settings.current(), launch, runtimes, failure, log),
      head_clip(clock),
      stereo_aa(settings.current(), log),
      reflections(keyboard, settings.current(), log),
      screen_policy(virtual_screen, keyboard, stereo, runtimes, log),
      mirror(platform::d3d11::MirrorObjects{manager.si.mirror_vs, manager.si.mirror_ps,
                                            manager.si.mirror_cb_screen, manager.si.mirror_cb,
                                            manager.si.mirror_sampler},
             &manager.si.eye_texture[0].srv, log),
      vignette(log),
      window(stereo, log),
      shader_fixes(log) {}

Context& create_context(void* module) noexcept {
    // Leaked on purpose: nothing of the mod runs at process exit (no
    // destructors under the loader lock).
    static Context& ctx = *new Context(module);
    the_context         = &ctx;

    // The debug setting the logger reports may change while the game runs.
    ctx.log.set_debug_flag(&ctx.settings.current().debug.debug);

    ctx.stereo.set_runtime_factory(
        [c = &ctx](core::vr::DetectResult::Chosen chosen) { return make_runtime(*c, chosen); });
    return ctx;
}

Context& context() noexcept { return *the_context; }

} // namespace omvr::loader
