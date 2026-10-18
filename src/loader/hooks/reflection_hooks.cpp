// OpenMotherVR-Provenance: clean
#include "loader/hooks/reflection_hooks.h"

#include "core/stereo/reflection_pass.h"
#include "game/code_address.h"
#include "game/stereo/renderer.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "platform/win32/com_ptr.h"
#include "platform/win32/process_info.h"

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <optional>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

using core::stereo::PixelViewport;
using core::stereo::ScreenMapping;
using game::DeviceState;
using game::StereoInterface;
using platform::win32::ComPtr;

// The game's functions these replace (07 §4.7, §4.8).
using UpdateFn = void(__fastcall*)(void* pov, void* edx);
static_assert(std::is_convertible_v<decltype(&reflection_update), UpdateFn>);
static_assert(std::is_convertible_v<decltype(&reflection_set_viewport), game::stereo::SetViewportFn>);
static_assert(
    std::is_convertible_v<decltype(&reflection_write_vs_constants), game::stereo::WriteVsConstantsFn>);

// The width in pixels of what is bound as render target 0 on `dc`: the
// mip level of the texture the view shows. Nullopt when nothing 2D is
// bound. A few interface calls, no allocation; only during the passes.
std::optional<std::int32_t> bound_target_width(ID3D11DeviceContext* dc) noexcept {
    if (!dc) return std::nullopt;
    ID3D11RenderTargetView* raw_rtv = nullptr;
    dc->OMGetRenderTargets(1, &raw_rtv, nullptr);
    if (!raw_rtv) return std::nullopt;
    const auto rtv = ComPtr<ID3D11RenderTargetView>::adopt(raw_rtv);
    D3D11_RENDER_TARGET_VIEW_DESC view{};
    rtv->GetDesc(&view);
    UINT mip = 0;
    if (view.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) {
        mip = view.Texture2D.MipSlice;
    } else if (view.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY) {
        mip = view.Texture2DArray.MipSlice;
    } else if (view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS &&
               view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY) {
        return std::nullopt;
    }
    ID3D11Resource* raw_res = nullptr;
    rtv->GetResource(&raw_res);
    const auto res               = ComPtr<ID3D11Resource>::adopt(raw_res);
    ID3D11Texture2D* raw_texture = nullptr;
    if (!res.get() || FAILED(res->QueryInterface(IID_PPV_ARGS(&raw_texture)))) return std::nullopt;
    const auto texture = ComPtr<ID3D11Texture2D>::adopt(raw_texture);
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    return static_cast<std::int32_t>(std::max<UINT>(desc.Width >> mip, 1));
}

// The half whose per-eye pass runs on the calling thread, if any.
std::optional<int> running_pass(Context& ctx) noexcept {
    return ctx.reflections.pass_on(platform::win32::current_thread_id());
}

} // namespace

void __fastcall reflection_update(void* pov, void*) noexcept {
    auto& ctx           = context();
    const auto& targets = ctx.targets.render_fixes.reflection;
    game::stereo::update_reflection(targets, pov);
    StereoInterface* dev = game::stereo::load_device(ctx.manager);
    const bool stereo    = dev && game::stereo::load_state(*dev) == DeviceState::Running;
    if (ctx.reflections.widens_object_list(stereo, game::stereo::pov_active(pov))) {
        game::stereo::widen_reflection_projection(targets, ctx.manager, pov);
    }
}

void __cdecl reflection_set_viewport(void* game_context,
                                     const game::stereo::GameViewport* viewport) noexcept {
    auto& ctx                      = context();
    const auto& targets            = ctx.targets.render_fixes.reflection;
    const std::optional<int> pass  = viewport ? running_pass(ctx) : std::nullopt;
    const std::optional<int> width = pass ? bound_target_width(static_cast<ID3D11DeviceContext*>(
                                                game::stereo::viewport_d3d_context(game_context)))
                                          : std::nullopt;
    if (!width) {
        targets.set_viewport_original(game_context, viewport); // as the game does
        return;
    }
    const PixelViewport in  = {viewport->x, viewport->y, viewport->width, viewport->height};
    const PixelViewport out = ctx.reflections.pass_viewport(*pass, in, *width);
    if (out == in) {
        targets.set_viewport_original(game_context, viewport);
        return;
    }
    game::stereo::GameViewport confined = *viewport; // the depth range kept
    confined.x                          = out.x;
    confined.width                      = out.width;
    targets.set_viewport_original(game_context, &confined);
}

void __cdecl reflection_write_vs_constants(void* renderer, std::int32_t buffer, std::int32_t first_register,
                                           const float* data, std::int32_t register_count) noexcept {
    auto& ctx           = context();
    const auto& targets = ctx.targets.render_fixes.reflection;
    targets.write_vs_constants_original(renderer, buffer, first_register, data, register_count);
    const float* mapping = game::stereo::screen_mapping_in(buffer, first_register, data, register_count);
    const std::optional<int> pass = mapping ? running_pass(ctx) : std::nullopt;
    if (!pass) return;
    const std::optional<int> width = bound_target_width(
        game::stereo::renderer_get<ID3D11DeviceContext>(ctx.addresses, game::AiAddr::RendererContext));
    if (!width) return;
    const ScreenMapping in  = {mapping[0], mapping[1], mapping[2], mapping[3]};
    const ScreenMapping out = core::stereo::confine_screen_mapping(in, *width, *pass);
    if (out == in) return;
    // Written after the game's values, so the register holds the mapping of
    // the viewport actually set (07 §4.8).
    const std::array<float, game::stereo::kFloat4Floats> value = {out.scale_x, out.scale_y, out.offset_x,
                                                                  out.offset_y};
    game::stereo::write_screen_mapping(targets, renderer, value.data());
}

void begin_reflection_pass(int eye) noexcept {
    context().reflections.begin_pass(eye, platform::win32::current_thread_id());
}

void end_reflection_pass(int) noexcept { context().reflections.end_pass(); }

game::stereo::ReflectionReplacements reflection_replacements() noexcept {
    return {.set_viewport       = &reflection_set_viewport,
            .write_vs_constants = &reflection_write_vs_constants,
            .update             = game::code_pointer(&reflection_update)};
}

} // namespace omvr::loader::hooks
