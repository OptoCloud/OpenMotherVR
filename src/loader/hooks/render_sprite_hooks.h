// OpenMotherVR-Provenance: clean
#pragma once

// Upright sprites in stereo (24 §14): ID3D11Device::CreateVertexShader
// rewrites the game's camera-facing sprite shaders as they are created.
namespace omvr::loader::hooks {

// Replaces CreateVertexShader in the device vtable `vtable` (shared by every
// device of the class; the fix only rewrites the game's own sprite form, so
// another device's shaders pass through unchanged). Logs once that it is in
// place, or that the slot could not be replaced.
void install_vertex_shader_hook(void** vtable) noexcept;

} // namespace omvr::loader::hooks
