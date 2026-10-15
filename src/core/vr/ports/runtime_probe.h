#pragma once

#include <filesystem>
#include <string>

namespace omvr::core::vr::ports {

// What a runtime probe found: whether the runtime can be started from this
// 32-bit process, where it is, and why not.
struct ProbeResult {
    bool                  available = false;
    std::filesystem::path location;
    std::string           why;
};

// Looks for one installed VR runtime without loading any of its DLLs.
// Game thread, at the first stereo device creation.
class RuntimeProbe {
public:
    virtual ProbeResult probe() = 0;

protected:
    ~RuntimeProbe() = default;
};

} // namespace omvr::core::vr::ports
