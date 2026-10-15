// The stereo manager and device the game is given in place of its own.
//
// From: FUN_10003510 / FUN_10003640 (init), FUN_1001eac0 (device init),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook), the
// StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300) and the
// StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290).
// Analysis: dumps/06a_stereo_interface.md.

#include "core/stereo/stereo_device.h"

#include "core/stereo/mirror_layout.h"
#include "core/stereo/render_target.h"
#include "core/ui/compat_messages.h"
#include "game/constants.h"
#include "game/stereo/stereo_objects.h" // layer exception: see stereo_device.h

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace omvr::core::stereo {

namespace {

using game::DeviceState;
using game::StereoInterface;
using game::StereoManager;
using game::load_backend;
using game::load_pose_fetched;
using game::store_backend;
using game::stereo::load_device;
using game::stereo::load_state;
using game::stereo::store_device;
using game::stereo::store_state;
using Chosen = vr::DetectResult::Chosen;

constexpr std::string_view kInitFailedLog   = "[stereo] HMD backend init failed, running without VR";
constexpr std::string_view kCannotRenderLog = "[stereo] HMD backend cannot render, running without VR";
constexpr std::string_view kNoDeviceLog     = "[stereo] no stereo device for runtime {}";
constexpr std::string_view kConfiguredLog   = "[stereo] rendering configured: {}x{} texture={}";
constexpr std::string_view kEyeOffsetsLog =
    "[stereo] eye offsets: eye0 ({:.4f} {:.4f} {:.4f}) eye1 ({:.4f} {:.4f} {:.4f})";
constexpr std::string_view kTargetSizeLog =
    "[stereo] render target size {}x{} ({:.1f} MP, pixel scale {:.2f}; state {}, texture {}x{})";
constexpr std::string_view kCreateDeviceLog = "[stereo] CreateDevice{}: {}";

// The runtime's eyes, as VrRuntime numbers them.
constexpr int kLeftEye  = 0;
constexpr int kRightEye = 1;
// The runtime's recommended size, before the pixel scale.
constexpr float kNativeScale = 1.0f;

// x, y, z of an eye offset.
constexpr std::size_t kAxes = 3;
// VrRuntime::begin_frame's eye offsets: [0..2] left eye, [3..5] right eye.
using EyeOffsets = std::array<float, 2 * kAxes>;

constexpr double kPixelsPerMegapixel = 1e6;

// The game's LibOVR 0.4 descriptor and the runtime-neutral one: what the
// runtime fills goes back into the game's.
vr::FovTangents tangents_of(const game::FovPort& f) noexcept { return {f.up, f.down, f.left, f.right}; }

game::FovPort fov_port_of(const vr::FovTangents& t) noexcept { return {t.up, t.down, t.left, t.right}; }

vr::HeadsetDesc headset_desc_of(const game::HmdDesc& d) noexcept {
    vr::HeadsetDesc h;
    h.product_name = d.product_name;
    h.manufacturer = d.manufacturer;
    for (std::size_t e = 0; e < vr::kEyeCount; ++e) {
        h.default_eye_fov[e] = tangents_of(d.default_eye_fov[e]);
        h.max_eye_fov[e]     = tangents_of(d.max_eye_fov[e]);
    }
    return h;
}

void apply_headset_desc(game::HmdDesc& d, const vr::HeadsetDesc& h) noexcept {
    d.product_name = h.product_name;
    d.manufacturer = h.manufacturer;
    for (std::size_t e = 0; e < vr::kEyeCount; ++e) {
        d.default_eye_fov[e] = fov_port_of(h.default_eye_fov[e]);
        d.max_eye_fov[e]     = fov_port_of(h.max_eye_fov[e]);
    }
}

// The game renders each eye with the symmetric FOV (03 §3.2).
void render_with_max_fov(StereoInterface& si) noexcept {
    si.eye_fov[0] = si.desc.max_eye_fov[0];
    si.eye_fov[1] = si.desc.max_eye_fov[1];
}

PixelSize target_size(const StereoInterface& si) noexcept { return {si.rt_width, si.rt_height}; }

PixelSize configured_size(const StereoInterface& si) noexcept {
    return {si.configured_width, si.configured_height};
}

game::Recti recti_of(const PixelRect& r) noexcept { return {r.x, r.y, r.width, r.height}; }

void set_target_size(StereoInterface& si, PixelSize size) noexcept {
    si.rt_width  = size.width;
    si.rt_height = size.height;
    const EyeViewports eyes = eye_viewports(size);
    si.viewport[0] = recti_of(eyes[0]);
    si.viewport[1] = recti_of(eyes[1]);
}

MirrorView mirror_view_of(const StereoInterface& si, const config::RenderingSettings& rendering) noexcept {
    return {.texture      = target_size(si),
            .window       = {si.mirror_width, si.mirror_height},
            .real_fov     = {tangents_of(si.desc.default_eye_fov[0]), tangents_of(si.desc.default_eye_fov[1])},
            .rendered_fov = {tangents_of(si.desc.max_eye_fov[0]), tangents_of(si.desc.max_eye_fov[1])},
            .type         = rendering.mirror_view_type,
            .symmetric    = rendering.symmetric_mirror_view};
}

// The runtime's eye offsets, scaled by -vrscale (03 §5). The left eye's
// offset goes to the game's eye 1, as in the original.
void apply_eye_offsets(StereoInterface& si, const EyeOffsets& eye) noexcept {
    const float s = si.world_scale;
    for (std::size_t i = 0; i < kAxes; ++i) {
        si.eye_desc[1].view_adjust[i] = s * eye[i];
        si.eye_desc[0].view_adjust[i] = s * eye[kAxes + i];
    }
}

// The settings' runtime id the device records for `chosen`, none for a
// runtime the device cannot drive.
std::optional<config::VRRuntime> runtime_id_of(Chosen chosen) noexcept {
    if (chosen == Chosen::OpenVR) return config::VRRuntime::OpenVR;
    if (chosen == Chosen::OpenXR) return config::VRRuntime::OpenXR;
    return std::nullopt;
}

} // namespace

bool StereoDevice::running() const noexcept {
    StereoInterface* dev = load_device(manager_);
    return dev && load_state(*dev) == DeviceState::Running;
}

bool StereoDevice::device_attached() const noexcept { return load_device(manager_) != nullptr; }

std::optional<PixelSize> StereoDevice::attached_window_size() const noexcept {
    const StereoInterface* dev = load_device(manager_);
    if (!dev) return std::nullopt;
    return PixelSize{dev->mirror_width, dev->mirror_height};
}

// The runtime cannot drive the headset after all: detach the device, as a
// failed init leaves it, and tell the player. State and pointer are cleared
// before the runtime, so no new call starts using it.
//
// At init (game thread, before the device was ever attached) nothing else
// can hold the runtime: it is destroyed, which shuts the runtime down. A
// late failure (slot 3, on the render thread) can come while the game
// thread is inside a call into the runtime (head pose, recenter): the
// runtime is then only taken off the device and kept, unused, until the
// process ends, so that call finishes on a live runtime.
void StereoDevice::fail_runtime(StereoInterface& si, Chosen chosen, bool late) noexcept {
    store_state(si, DeviceState::None);
    if (load_device(manager_) == &si) store_device(manager_, nullptr);
    vr_session_active_ = false;
    store_backend(si, nullptr);
    if (!late) runtime_.reset();
    runtime_failed_ = true;
    ui::report_init_failure(dialogs_, chosen, failure_.last());
}

// From: FUN_1001eac0.
void StereoDevice::init_device(StereoInterface& si) noexcept {
    store_state(si, DeviceState::None);
    si.frame_index = 0;
    si.hmd         = nullptr;
    si.rt_width = si.rt_height = 0;

    const Chosen chosen     = runtimes_.last_result().chosen;
    const auto   runtime_id = runtime_id_of(chosen);
    if (!runtime_id) {
        diag::ports::logf(log_, kNoDeviceLog, vr::to_string(chosen));
        return;
    }
    runtime_      = make_runtime_(chosen);
    si.runtime_id = static_cast<std::int32_t>(*runtime_id);
    store_backend(si, runtime_.get());
    if (!runtime_->init()) {
        log_.line(kInitFailedLog);
        fail_runtime(si, chosen, false);
        return;
    }
    runtime_->register_control_hints();

    game::stereo::describe_as_dk2(si);
    vr::HeadsetDesc desc = headset_desc_of(si.desc);
    runtime_->headset_desc(desc);
    apply_headset_desc(si.desc, desc);
    render_with_max_fov(si);
    store_state(si, DeviceState::Ready);
}

bool StereoDevice::configure_rendering(StereoInterface& si, std::optional<PixelSize> texture_size,
                                       vr::GpuDevice device, vr::GpuContext context, ports::DesktopMirror& mirror) {
    auto& t0    = si.eye_texture[0];
    t0.api      = game::kEyeTextureApiD3d11;
    t0.size     = {si.rt_width, si.rt_height};
    t0.viewport = si.viewport[0];
    // 03 §4.3: the eye halves must match the texture the game actually
    // created. The original trusts its last size computation, which runs at
    // half scale before stereo starts and at full scale after, so the two
    // can disagree.
    if (texture_size) {
        set_target_size(si, *texture_size);
        si.configured_width  = si.rt_width;
        si.configured_height = si.rt_height;
        t0.size     = {si.rt_width, si.rt_height};
        t0.viewport = si.viewport[0];
    }
    si.eye_texture[1]          = t0;
    si.eye_texture[1].viewport = si.viewport[1];

    if (auto* runtime = load_backend(si); runtime && !runtime->create_render_resources(device, context)) {
        log_.line(kCannotRenderLog);
        fail_runtime(si, runtimes_.last_result().chosen, true);
    }

    const MirrorView view = mirror_view_of(si, settings_.rendering);
    mirror.create(device, mirror_constants(view, false), mirror_constants(view, true));

    diag::ports::logf(log_, kConfiguredLog, si.rt_width, si.rt_height, static_cast<void*>(t0.texture));
    return true;
}

void StereoDevice::begin_frame(StereoInterface& si) {
    auto* runtime = load_backend(si);
    if (!runtime) return;
    runtime->poll_input();
    if (vr::HeadsetDesc desc = headset_desc_of(si.desc); runtime->refresh_headset_desc(desc)) {
        apply_headset_desc(si.desc, desc);
        render_with_max_fov(si);
    }
    EyeOffsets eye{};
    runtime->begin_frame(eye.data(), load_pose_fetched(si));
    apply_eye_offsets(si, eye);
    if (eye_offsets_logged_.first()) {
        const auto& a = si.eye_desc[0].view_adjust;
        const auto& b = si.eye_desc[1].view_adjust;
        diag::ports::logf(log_, kEyeOffsetsLog, a[0], a[1], a[2], b[0], b[1], b[2]);
    }
}

PixelSize StereoDevice::render_target_size(StereoInterface& si, vr::ports::VrRuntime& runtime) {
    const float scale = capped_pixel_scale(settings_.rendering.pixel_scale,
                                           runtime.eye_texture_size(kLeftEye, kNativeScale),
                                           runtime.eye_texture_size(kRightEye, kNativeScale));
    const PixelSize size =
        side_by_side(runtime.eye_texture_size(kLeftEye, scale), runtime.eye_texture_size(kRightEye, scale));
    if (layout_follows(size, configured_size(si))) set_target_size(si, size);
    if (size != logged_size_) {
        logged_size_ = size;
        diag::ports::logf(log_, kTargetSizeLog, size.width, size.height,
                          static_cast<double>(size.width) * size.height / kPixelsPerMegapixel, scale,
                          static_cast<int>(load_state(si)), si.configured_width, si.configured_height);
    }
    return size;
}

void StereoDevice::create_device(StereoManager& self) {
    auto& si = self.si;
    // The game can call this again (a display reset). A second VR_Init or
    // overlay would fail, and a runtime that failed has told the player
    // already: start the runtime once, then re-attach what it gave (03 §6.5).
    const bool first = !std::exchange(created_, true);
    if (first) {
        si.world_scale = switches_.world_scale();
        init_device(si);
    }
    StereoInterface* device = load_state(si) != DeviceState::None ? &si : nullptr;
    store_device(self, device);
    vr_session_active_ = device != nullptr;
    diag::ports::logf(log_, kCreateDeviceLog, first ? "" : " again", device ? "HMD attached" : "no HMD");
    if (first && device && load_state(si) == DeviceState::Ready) {
        game::stereo::recenter(self, si);
        // The headset may be lying on the desk at launch: take the
        // reference again once it is worn (a pose that was not valid has
        // queued the retake already).
        if (auto* runtime = load_backend(si); runtime && !runtime->headset_in_use()) update_.request_recenter();
    }
}

} // namespace omvr::core::stereo
