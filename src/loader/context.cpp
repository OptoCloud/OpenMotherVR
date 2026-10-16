// OpenMotherVR-Provenance: clean
#include "loader/context.h"

#include "core/ui/ui_contract.h"
#include "loader/hooks/haptic_hooks.h"
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
// Spec 21 §1, §16: the hands' aim pose actions only when Hands.Enabled is
// on as the runtime starts; 21 §14.3: the haptic output only when
// Hands.Haptics.Enabled was on at attach and its game sources went in.
std::unique_ptr<core::vr::ports::VrRuntime> make_runtime(Context& c, core::vr::DetectResult::Chosen chosen) {
    const bool aim_poses = c.settings.current().hands.enabled;
    core::vr::ports::HapticSource* haptics =
        c.settings.current().hands.haptics.enabled && hooks::haptics_installed() ? &c.haptics : nullptr;
    if (chosen == core::vr::DetectResult::Chosen::OpenVR) {
        return platform::openvr::make_openvr_runtime({c.log, c.clock, c.failure, c.archive_files, c.controls,
                                                      c.dialogs, c.runtimes.last_result().openvr_runtime_path,
                                                      aim_poses, haptics});
    }
    return platform::openxr::make_openxr_runtime(
        {c.log, c.xr, c.controls, c.headset_audio, c.console, aim_poses, haptics});
}

} // namespace

Context::Context(void* self) noexcept
    : module(self),
      flush_schedule(thread_roles),
      log_flusher(log_sink, clock, flush_schedule, log),
      log_out(log_sink, log_flusher, current_thread, thread_roles),
      log(clock, log_out, platform::win32::current_process_id()),
      crash(log),
      settings(files, log),
      dialogs(core::ui::kCaption, log),
      archive_files(self, OMVR_IDR_OPENVR_API, OMVR_IDR_UI_PATCH, log),
      ui_archive(archive_files, dialogs, log),
      runtime_state(runtimes, stereo, launch),
      play_space_control(calibration, clock, launch),
      display_reset(console, runtime_state, log),
      schemes(glyphs),
      pad(glyphs, log),
      controls(pad, schemes, settings.current().input),
      interactions(log),
      hands(settings.current().hands, log),
      haptics(settings.current().hands.haptics, clock, log),
      settings_push(
          {script_vars, display_reset, schemes, settings.current(), runtime_state, play_space_control, log}),
      commands(settings, display_reset, console, play_space_control, log),
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
      play_space(log),
      calibration(log),
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
