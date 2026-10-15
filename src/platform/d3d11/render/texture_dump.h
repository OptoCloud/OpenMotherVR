// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <filesystem>

struct ID3D11DeviceContext;
struct ID3D11Resource;

// Debug: write mip 0 of a 2D texture to a .dds file (DX10 header, the
// texture's own format). scripts/dds2png.py converts it for viewing. File
// I/O on the calling thread: only behind DebugOptions, on a key press.
namespace omvr::platform::d3d11 {

// A relative `path` is taken from the game folder (the module directory):
// the current directory is wherever Steam started the game. Logs the
// outcome; false if nothing was written.
bool dump_texture(ID3D11DeviceContext* ctx, ID3D11Resource* texture, const std::filesystem::path& path,
                  core::diag::ports::Log& log) noexcept;

// Debug (F10, 07 §6): write what the reflection pass left bound as render
// target to <game dir>/mothervr_reflection_<pass>.dds.
void dump_bound_target(ID3D11DeviceContext* ctx, int pass, core::diag::ports::Log& log);

} // namespace omvr::platform::d3d11
