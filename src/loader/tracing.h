// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::loader {

// Starts the profiler client of the traced flavour (MOTHERVR_TRACY) and
// names the calling thread, the game's. Call once, at the first call into
// the mod after attach that is outside the loader lock. Does nothing in the
// release flavour.
void start_tracing() noexcept;

} // namespace omvr::loader
