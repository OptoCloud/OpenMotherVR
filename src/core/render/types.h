// OpenMotherVR-Provenance: clean
#pragma once

// Value types of core/render that game/ and platform/ may use.
namespace omvr::core::render {

// The three keys (24 §10). Each default is the game's behaviour (v1).
struct PostFxKeys {
    bool lens_flares          = true;
    bool chromatic_aberration = true;
    bool film_grain           = true;

    friend bool operator==(const PostFxKeys&, const PostFxKeys&) = default;
};

// Which effects the mod turns off for this frame.
struct PostFxOff {
    bool lens_flares          = false;
    bool chromatic_aberration = false;
    bool film_grain           = false;

    friend bool operator==(const PostFxOff&, const PostFxOff&) = default;
};

} // namespace omvr::core::render
