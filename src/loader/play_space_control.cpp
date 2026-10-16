// OpenMotherVR-Provenance: clean
#include "loader/play_space_control.h"

namespace omvr::loader {

void PlaySpaceControlView::request_height_calibration() noexcept {
    log_.line("[script_bridge] standing height calibration is not available in this build");
}

} // namespace omvr::loader
