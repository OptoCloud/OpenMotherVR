// OpenMotherVR-Provenance: clean
#pragma once

// The control mapper over a fresh virtual pad, for the input tests: every
// setting at its default and the body gestures wired in (each off until a
// test turns its key on).

#include "core/config/settings.h"
#include "core/input/action_tiers.h"
#include "core/input/control_mapper.h"
#include "core/input/control_scheme.h"
#include "core/input/gestures.h"
#include "core/input/glyphs.h"
#include "core/input/virtual_pad.h"
#include "fakes/fakes.h"
#include "game/constants.h"

namespace omvr::tests {

struct MapperRig {
    core::input::Glyphs glyphs;
    RecordingLog log;
    core::input::VirtualPad pad{glyphs, log};
    core::input::ControlSchemes schemes{glyphs};
    core::config::InputSettings input;
    core::config::ControlsSettings controls;
    SettableStick stick;
    FakeClock clock;
    FakeGestureContext context;
    core::input::Gestures gestures{{controls, context, log}};
    core::input::ControlMapper mapper{{pad, schemes, input, controls, stick, clock, log, &gestures}};

    const core::input::ActionTier& player() { return pad.tiers()[game::kPlayerMap]; }
};

} // namespace omvr::tests
