// OpenMotherVR-Provenance: clean
// ID3D11Device::CreateVertexShader for the upright sprites (24 §14). The game
// creates its shaders when a level loads, on whichever thread loads it; the
// rewrite runs there, never per frame.

#include "loader/hooks/render_sprite_hooks.h"

#include "core/render/upright_sprites.h"
#include "loader/context.h"
#include "platform/d3d11/constants.h"
#include "platform/win32/patching/memory.h"

#include <windows.h>

#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

constexpr std::string_view kVertexShaderLog = "proxy: CreateVertexShader hooked (upright sprites)";

// ID3D11Device* this, bytecode, size, ID3D11ClassLinkage*, ID3D11VertexShader** out.
using CreateVertexShaderFn = HRESULT(__stdcall*)(void* self, const void* code, SIZE_T size, void* linkage,
                                                 void** out);

HRESULT __stdcall hook_create_vertex_shader(void* self, const void* code, SIZE_T size, void* linkage,
                                            void** out) noexcept {
    auto& ctx         = context();
    const auto create = reinterpret_cast<CreateVertexShaderFn>(
        ctx.targets.create_vertex_shader.load(std::memory_order_acquire));
    auto& stats        = ctx.sprite_fixes;
    const int total    = stats.count_created();
    int sites          = 0;
    const auto patched = core::render::patch_upright_sprites(code, size, sites);
    if (!patched.empty()) {
        const HRESULT hr = create(self, patched.data(), patched.size(), linkage, out);
        if (SUCCEEDED(hr)) {
            stats.count_fixed(total);
            return hr;
        }
        stats.count_rejected(hr);
    }
    return create(self, code, size, linkage, out);
}

static_assert(std::is_convertible_v<decltype(&hook_create_vertex_shader), CreateVertexShaderFn>);

} // namespace

void install_vertex_shader_hook(void** vtable) noexcept {
    auto& ctx = context();
    if (platform::win32::replace_pointer(&vtable[platform::d3d11::kCreateVertexShaderVtblIndex],
                                         reinterpret_cast<void*>(&hook_create_vertex_shader),
                                         ctx.targets.create_vertex_shader, "CreateVertexShader", ctx.log)) {
        ctx.log.line(kVertexShaderLog);
    }
}

} // namespace omvr::loader::hooks
