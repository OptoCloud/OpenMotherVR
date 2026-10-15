// OpenMotherVR-Provenance: clean
#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

namespace omvr::platform::discord {

// The Discord application whose name the profile shows: Discord writes
// "Playing <application name>" and a client cannot change it, so the
// application is named "Alien: Isolation VR". Created at
// https://discord.com/developers/applications. Empty: Rich Presence stays
// off in this build.
inline constexpr std::string_view kApplicationId = "1560307824381263962";

// Art key uploaded under the application's Rich Presence assets, and its
// hover text. A key Discord does not know shows no image.
inline constexpr std::string_view kLargeImage = "logo";
inline constexpr std::string_view kLargeText  = "Alien: Isolation VR";

// The client listens on the first free of discord-ipc-0 .. -9.
inline constexpr int kPipeCount = 10;

// IPC frame opcodes.
inline constexpr std::uint32_t kOpHandshake = 0;
inline constexpr std::uint32_t kOpFrame     = 1;
inline constexpr std::uint32_t kOpClose     = 2;
inline constexpr std::uint32_t kOpPing      = 3;
inline constexpr std::uint32_t kOpPong      = 4;

inline constexpr int kProtocolVersion = 1;

// A frame larger than this is not a reply to us; the connection is dropped.
inline constexpr std::uint32_t kMaxFrame = 64 * 1024;

// How often the activity source is polled, and how long to wait before
// looking for Discord again after it was not found or went away.
inline constexpr std::chrono::seconds kPollPeriod{5};
inline constexpr std::chrono::seconds kRetryPeriod{30};

} // namespace omvr::platform::discord
