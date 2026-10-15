#pragma once

#include "core/stereo/types.h"
#include "core/vr/types.h"

namespace omvr::core::stereo::ports {

// The desktop window's picture: the eyes or the virtual screen's region of
// the stereo texture. Render thread.
class DesktopMirror {
public:
    virtual void create(vr::GpuDevice device, const MirrorConstants& eyes, const MirrorConstants& screen) noexcept = 0;
    virtual void draw(vr::GpuContext context, bool head_tracked) noexcept = 0;

protected:
    ~DesktopMirror() = default;
};

} // namespace omvr::core::stereo::ports
