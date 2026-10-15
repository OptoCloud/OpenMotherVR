#pragma once

#include <cstdint>

namespace omvr::core::ui::ports {

// The game's menu object a settings push runs on (the `this` of the menu
// method that opened it).
enum class UiHandle : std::uintptr_t {};

// The game's UI script variables, set by name on a menu object. Game thread.
class ScriptVariables {
public:
    // The setters every push needs are known, so early or late calls are safe.
    virtual bool ready() const noexcept                                         = 0;
    virtual void set_bool(UiHandle ui, const char* name, bool value) noexcept   = 0;
    virtual void set_int(UiHandle ui, const char* name, int value) noexcept     = 0;
    virtual void set_float(UiHandle ui, const char* name, float value) noexcept = 0;
    // Registers a row with two display strings. False when the game's
    // function for it is not known (nothing is called).
    virtual bool add_row(UiHandle ui, const char* name, const char* a, const char* b) noexcept = 0;

protected:
    ~ScriptVariables() = default;
};

} // namespace omvr::core::ui::ports
