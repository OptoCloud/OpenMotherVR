#include "openxr_backend.h"

#include "openxr_input.h"
#include "vr_detect.h"
#include "config/config.h"
#include "diag/log.h"
#include "integration/dialogs.h"

#include <nlohmann/json.hpp>
#include <openxr/openxr_reflection.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace mothervr::runtime::openxr {

namespace {

// ---- Runtime discovery --------------------------------------------------------

std::filesystem::path active_runtime(REGSAM view) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Khronos\\OpenXR\\1", 0,
                        KEY_READ | view, &key) != ERROR_SUCCESS) {
        return {};
    }
    // A path of whatever length the registry holds (09 §1): ask for the size
    // first instead of assuming MAX_PATH.
    DWORD type = 0, size = 0;
    LSTATUS st = ::RegQueryValueExW(key, L"ActiveRuntime", nullptr, &type, nullptr, &size);
    std::wstring value;
    if (st == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0) {
        value.resize(size / sizeof(wchar_t) + 1);   // room for a missing terminator
        DWORD got = static_cast<DWORD>(value.size() * sizeof(wchar_t));
        st = ::RegQueryValueExW(key, L"ActiveRuntime", nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &got);
    }
    ::RegCloseKey(key);
    if (st != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    value.resize(::wcsnlen(value.c_str(), value.size()));
    // An expandable value names its folder through environment references
    // (%ProgramFiles%...); the loader expands them, so must we, or the
    // manifest is not found.
    if (type == REG_EXPAND_SZ && !value.empty()) {
        if (const DWORD n = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0); n > 0) {
            std::wstring expanded(n, L'\0');
            const DWORD m = ::ExpandEnvironmentStringsW(value.c_str(), expanded.data(), n);
            if (m > 0 && m <= n) {
                expanded.resize(m - 1);
                value = std::move(expanded);
            }
        }
    }
    return value;
}

// IMAGE_FILE_HEADER.Machine of a PE file, 0 if unreadable.
WORD pe_machine(const std::filesystem::path& file) {
    std::ifstream f(file, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!f.read(reinterpret_cast<char*>(&dos), sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    f.seekg(dos.e_lfanew);
    DWORD sig = 0;
    IMAGE_FILE_HEADER fh{};
    if (!f.read(reinterpret_cast<char*>(&sig), sizeof(sig)) || sig != IMAGE_NT_SIGNATURE) return 0;
    if (!f.read(reinterpret_cast<char*>(&fh), sizeof(fh))) return 0;
    return fh.Machine;
}

// A manifest is usable from this process if its runtime library is x86.
bool usable_from_x86(const std::filesystem::path& manifest) {
    std::error_code ec;
    if (manifest.empty() || !std::filesystem::exists(manifest, ec)) return false;
    try {
        std::ifstream f(manifest);
        const auto j = nlohmann::json::parse(f);
        const auto lib_utf8 = j.at("runtime").at("library_path").get<std::string>();
        std::filesystem::path lib{std::u8string(lib_utf8.begin(), lib_utf8.end())};
        if (lib.is_relative()) lib = manifest.parent_path() / lib;
        const WORD m = pe_machine(lib);
        diag::logf("openxr: manifest {} -> {} (machine 0x{:04X})", diag::utf8(manifest), diag::utf8(lib), m);
        return m == IMAGE_FILE_MACHINE_I386;
    } catch (const std::exception& e) {
        diag::logf("openxr: cannot read manifest {}: {}", diag::utf8(manifest), e.what());
        return false;
    }
}

// 32-bit manifests usually sit next to the 64-bit one with "32" in place of
// "64" in the name (steamxr_win64.json -> steamxr_win32.json,
// oculus_openxr_64.json -> oculus_openxr_32.json).
std::vector<std::filesystem::path> x86_siblings(const std::filesystem::path& manifest64) {
    std::vector<std::filesystem::path> out;
    std::wstring name = manifest64.filename().wstring();
    for (const auto& [from, to] : {std::pair{L"win64", L"win32"}, std::pair{L"x64", L"x86"},
                                   std::pair{L"64", L"32"}}) {
        if (const auto pos = name.rfind(from); pos != std::wstring::npos) {
            std::wstring n = name;
            n.replace(pos, std::wcslen(from), to);
            out.push_back(manifest64.parent_path() / n);
        }
    }
    return out;
}

// The manifest probe() chose; init_system hands it to the loader.
std::filesystem::path g_manifest;

bool process_elevated() noexcept {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD size = 0;
    const BOOL ok = ::GetTokenInformation(token, TokenElevation, &e, sizeof(e), &size);
    ::CloseHandle(token);
    return ok && e.TokenIsElevated;
}

// The loader ignores XR_RUNTIME_JSON in an elevated process; a loader
// property (XR_EXT_loader_init_properties) is honoured there too. Must run
// before the first other loader call.
void point_loader_at(const std::filesystem::path& manifest) noexcept {
    if (manifest.empty()) return;
    PFN_xrInitializeLoaderKHR init_loader = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&init_loader))) ||
        !init_loader) {
        diag::log_line("openxr: loader has no xrInitializeLoaderKHR; only XR_RUNTIME_JSON points it at the manifest");
        return;
    }
    const std::string path = diag::utf8(manifest);
    XrLoaderInitPropertyValueEXT value{"XR_RUNTIME_JSON", path.c_str()};
    XrLoaderInitInfoPropertiesEXT props{XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT};
    props.propertyValueCount = 1;
    props.propertyValues     = &value;
    const XrResult r = init_loader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&props));
    // Either outcome is logged (09 §1): this is the first thing to check
    // when "OpenXR does not start" is reported from an elevated game.
    if (XR_FAILED(r)) {
        diag::logf("openxr: loader property not set: {}; only XR_RUNTIME_JSON points the loader at {}",
                   static_cast<int>(r), path);
    } else {
        diag::logf("openxr: loader directed to {} (initialisation property accepted)", path);
    }
}

// Hands the chosen manifest to the loader through the environment (this
// module's CRT copy and the process), only once OpenXR is the runtime that
// starts: a probe for a runtime the player did not choose must not leave the
// variable to the game's child processes (01 §5.2). The elevated line goes
// with it, since only then is the loader property the one that counts.
void set_runtime_environment(const std::filesystem::path& manifest) noexcept {
    if (manifest.empty()) return;
    _wputenv_s(L"XR_RUNTIME_JSON", manifest.c_str());
    static bool s_elevation_logged = false;
    if (!s_elevation_logged && process_elevated()) {
        s_elevation_logged = true;
        diag::log_line("openxr: the game runs elevated (as administrator); the loader ignores the environment, "
                       "only its initialisation property points it at the runtime");
    }
}

// ---- Session state ------------------------------------------------------------

// The last result of each call made on a swapchain every frame, so a call
// that keeps failing is logged when its result changes, not every frame
// (11 §3). Kept when the swapchain is destroyed and recreated.
struct ChainResults {
    XrResult create  = XR_SUCCESS;
    XrResult images  = XR_SUCCESS;
    XrResult acquire = XR_SUCCESS;
    XrResult wait    = XR_SUCCESS;
    XrResult release = XR_SUCCESS;
    DXGI_FORMAT no_format = DXGI_FORMAT_UNKNOWN;   // source format already logged as unmatched
};

struct Swapchain {
    XrSwapchain handle = XR_NULL_HANDLE;
    std::uint32_t width = 0, height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    std::vector<XrSwapchainImageD3D11KHR> images;
    ChainResults results{};
};

// Where the virtual screen goes when no pose is set (set_screen_pose): this
// far ahead of the seated origin and this wide, in metres.
constexpr float kDefaultScreenDistance = 2.0f;
constexpr float kDefaultScreenWidth    = 2.6f;

struct State {
    XrInstance   instance = XR_NULL_HANDLE;
    XrSystemId   system   = XR_NULL_SYSTEM_ID;
    XrSession    session  = XR_NULL_HANDLE;
    XrSpace      local    = XR_NULL_HANDLE; // runtime's LOCAL origin
    XrSpace      seated   = XR_NULL_HANDLE; // LOCAL re-anchored by recenter()
    XrSpace      view     = XR_NULL_HANDLE;
    XrSessionState state  = XR_SESSION_STATE_UNKNOWN;
    bool         running  = false;
    bool         lost     = false;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    std::vector<std::int64_t> formats;
    std::array<XrViewConfigurationView, 2> config_views{};
    // The frame begun by the last frame() and not yet ended.
    bool         frame_begun   = false;
    bool         should_render = false;
    XrTime       frame_time    = 0; // its predictedDisplayTime
    XrDuration   frame_period  = 0; // its predictedDisplayPeriod
    bool         recenter_pending = true; // anchor once tracking is up
    bool         exit_requested = false;

    ID3D11Device*        device  = nullptr;
    ID3D11DeviceContext* context = nullptr;

    std::array<Swapchain, 2> eye_chain{};
    Swapchain                screen_chain{};

    EyeImages    eye_images{};
    bool         eye_images_set = false;
    FrameViews   views{};    // the begun frame's views (render thread's copy)
    ID3D11Texture2D* screen_image = nullptr;
    D3D11_BOX        screen_region{};
    bool             screen_pose_set = false;
    Pose             screen_pose{};
    float            screen_width = 0.0f;
    SystemInfo       info{};
    bool         ipd_logged = false;

    // The last valid head and eye positions in the base space (09 §5.5):
    // what is published while tracking is lost. Neutral before any: the
    // identity head, the eyes half an average IPD to each side.
    Pose                 last_head{};
    std::array<Vec3, 2>  last_eye_pos{{{-0.032f, 0.0f, 0.0f}, {0.032f, 0.0f, 0.0f}}};
    float                anchor_yaw = 0.0f;   // the base space's yaw at the latest anchor

    // The per-frame calls' last results, logged on change (11 §3).
    XrResult     wait_result  = XR_SUCCESS;
    XrResult     begin_result = XR_SUCCESS;
    XrResult     end_result   = XR_SUCCESS;

    // Pose-age probe: how many display periods before the frame's own
    // display time the declared (and rendered) views were predicted for.
    // 0 is right; the old loop measured 1. Logged with Dev. Debug Options.
    std::uint32_t age_frames = 0, age_same = 0, age_one = 0, age_other = 0, age_known = 0;
    double        age_sum    = 0.0;
} g;

// g.views as published to other threads. Outside State: atomics do not
// reset by assignment.
SeqLock<FrameViews> g_views;

constexpr std::uint32_t kPoseAgeLogFrames = 900;

void count_pose_age(XrTime rendered_for) {
    // Unknown: no prediction recorded for the views, or no display period.
    const bool known = g.frame_period > 0 && rendered_for != 0;
    const double periods = known
                               ? static_cast<double>(g.frame_time - rendered_for) / static_cast<double>(g.frame_period)
                               : 0.0;
    const long long whole = std::llround(periods);
    if (known && whole == 0) ++g.age_same;
    else if (known && whole == 1) ++g.age_one;
    else ++g.age_other;
    if (known) {
        g.age_sum += periods;
        ++g.age_known;
    }
    if (++g.age_frames < kPoseAgeLogFrames) return;
    if (config::current().debug.debug) {
        // The mean is over the frames whose age is known (11 §4.1).
        diag::logf("openxr: pose age over {} frames: {} same frame, {} one period early, {} other, "
                   "mean {:.2f} periods over {} known",
                   g.age_frames, g.age_same, g.age_one, g.age_other,
                   g.age_known ? g.age_sum / g.age_known : 0.0, g.age_known);
    }
    g.age_frames = g.age_same = g.age_one = g.age_other = g.age_known = 0;
    g.age_sum = 0.0;
}

// xrResultToString needs an instance, and there is none when xrCreateInstance
// itself fails; the names come from the registry header instead.
const char* result_name(XrResult r) noexcept {
    switch (r) {
#define MVR_XR_RESULT_CASE(name, value) case name: return #name;
        XR_LIST_ENUM_XrResult(MVR_XR_RESULT_CASE)
#undef MVR_XR_RESULT_CASE
        default: return "unknown XrResult";
    }
}

// The result's text, never empty (09 §7).
void result_text(XrResult r, char (&text)[XR_MAX_RESULT_STRING_SIZE]) noexcept {
    text[0] = '\0';
    if (g.instance != XR_NULL_HANDLE) xrResultToString(g.instance, r, text);
    if (text[0] == '\0') strncpy_s(text, result_name(r), _TRUNCATE);
}

// Start-up and session calls: a failure is logged and becomes the reason the
// player's start-up message gives.
bool check(XrResult r, const char* what) {
    if (XR_SUCCEEDED(r)) return true;
    char text[XR_MAX_RESULT_STRING_SIZE];
    result_text(r, text);
    diag::logf("openxr: {} failed: {} ({})", what, static_cast<int>(r), text);
    set_init_failure(std::string{what} + ": " + text, static_cast<int>(r));
    return false;
}

// Calls made every frame (frame functions, swapchain images): logged when
// the result changes, never on every frame, and without allocating (11 §3,
// 12 §6.1). A failure during frames is not a start-up failure, so the
// player's start-up message is left alone.
bool check_frame(XrResult r, const char* what, XrResult& last) noexcept {
    if (r != last) {
        if (XR_FAILED(r)) {
            char text[XR_MAX_RESULT_STRING_SIZE];
            result_text(r, text);
            diag::logf("openxr: {} failed: {} ({})", what, static_cast<int>(r), text);
        } else if (XR_FAILED(last)) {
            diag::logf("openxr: {} succeeds again", what);
        }
        last = r;
    }
    return XR_SUCCEEDED(r);
}

// Swapchain format whose images the source can be copied into
// (CopySubresourceRegion needs the same typeless family). sRGB variants are
// preferred: the game's back buffer holds gamma-encoded values, and an sRGB
// swapchain tells the compositor exactly that.
DXGI_FORMAT pick_format(DXGI_FORMAT src) {
    std::vector<DXGI_FORMAT> want;
    switch (src) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            want = {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM};
            break;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            want = {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM};
            break;
        default:
            want = {src};
    }
    for (DXGI_FORMAT f : want) {
        for (std::int64_t have : g.formats) {
            if (have == static_cast<std::int64_t>(f)) return f;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}

void destroy_chain(Swapchain& sc) {
    if (sc.handle != XR_NULL_HANDLE) xrDestroySwapchain(sc.handle);
    const ChainResults results = sc.results;
    sc = {};
    sc.results = results;
}

bool ensure_chain(Swapchain& sc, DXGI_FORMAT src_format, std::uint32_t w, std::uint32_t h) {
    const DXGI_FORMAT fmt = pick_format(src_format);
    if (fmt == DXGI_FORMAT_UNKNOWN) {
        // Asked every frame; said once per game format (09 §7).
        if (sc.results.no_format != src_format) {
            sc.results.no_format = src_format;
            diag::logf("openxr: runtime has no swapchain format compatible with {}; no layer will be submitted",
                       static_cast<int>(src_format));
        }
        return false;
    }
    if (sc.handle != XR_NULL_HANDLE && sc.format == fmt && sc.width == w && sc.height == h) return true;
    destroy_chain(sc);

    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    ci.format      = fmt;
    ci.sampleCount = 1;
    ci.width       = w;
    ci.height      = h;
    ci.faceCount   = 1;
    ci.arraySize   = 1;
    ci.mipCount    = 1;
    // Retried every frame while it fails: logged on change only.
    if (!check_frame(xrCreateSwapchain(g.session, &ci, &sc.handle), "xrCreateSwapchain", sc.results.create)) {
        sc.handle = XR_NULL_HANDLE;
        return false;
    }

    std::uint32_t n = 0;
    xrEnumerateSwapchainImages(sc.handle, 0, &n, nullptr);
    sc.images.assign(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    if (!check_frame(xrEnumerateSwapchainImages(sc.handle, n, &n,
                         reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data())),
                     "xrEnumerateSwapchainImages", sc.results.images)) {
        destroy_chain(sc);
        return false;
    }
    sc.width = w;
    sc.height = h;
    sc.format = fmt;
    diag::logf("openxr: swapchain {}x{} fmt={} images={}", w, h, static_cast<int>(fmt), n);
    return true;
}

// Copy `src` (region `box`) into the next image of `sc`.
bool copy_into(Swapchain& sc, ID3D11Texture2D* src, D3D11_BOX box) {
    box.right  = (std::min)(box.right,  box.left + sc.width);
    box.bottom = (std::min)(box.bottom, box.top + sc.height);
    box.front = 0;
    box.back  = 1;

    std::uint32_t index = 0;
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (!check_frame(xrAcquireSwapchainImage(sc.handle, &ai, &index), "xrAcquireSwapchainImage",
                     sc.results.acquire)) {
        return false;
    }
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = XR_INFINITE_DURATION;
    const XrResult wr = xrWaitSwapchainImage(sc.handle, &wi);
    // A timeout is a success code but leaves the image unwritten.
    const bool waited = check_frame(wr, "xrWaitSwapchainImage", sc.results.wait) && wr != XR_TIMEOUT_EXPIRED;
    if (waited) g.context->CopySubresourceRegion(sc.images[index].texture, 0, 0, 0, 0, src, 0, &box);
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const bool released = check_frame(xrReleaseSwapchainImage(sc.handle, &ri), "xrReleaseSwapchainImage",
                                      sc.results.release);
    // An image that was not written must not be shown: the frame then ends
    // with no layer (09 §5.2).
    return waited && released;
}

bool all_finite(const Pose& p) noexcept {
    const float v[7] = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w,
                        p.position.x, p.position.y, p.position.z};
    for (float f : v)
        if (!std::isfinite(f)) return false;
    return true;
}

// `space` in `base` at `t`, and its location flags; flags 0 when the call
// fails or the pose is not finite, so nothing NaN is ever handed on (09 §5.5).
XrSpaceLocationFlags locate_flags(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(space, base, t, &loc))) return 0;
    const Pose p = from_xr(loc.pose);
    if (!all_finite(p)) return 0;
    out = p;
    return loc.locationFlags;
}

constexpr XrSpaceLocationFlags kOrientationAndPosition =
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;

// Both orientation and position valid: what a re-anchor needs.
bool locate(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    Pose p{};
    if ((locate_flags(space, base, t, p) & kOrientationAndPosition) != kOrientationAndPosition) return false;
    out = p;
    return true;
}

XrSpace make_reference_space(XrReferenceSpaceType type, const XrPosef& pose) {
    XrReferenceSpaceCreateInfo ci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    ci.referenceSpaceType   = type;
    ci.poseInReferenceSpace = pose;
    XrSpace s = XR_NULL_HANDLE;
    check(xrCreateReferenceSpace(g.session, &ci, &s), "xrCreateReferenceSpace");
    return s;
}

constexpr XrPosef kIdentity{{0, 0, 0, 1}, {0, 0, 0}};

// Seated origin at the head's current position, facing its yaw (pitch and
// roll ignored so the world stays level).
bool do_recenter(XrTime t) {
    Pose head{};
    if (!locate(g.view, g.local, t, head)) return false;   // stays pending (09 §6.3)
    const Quat& q = head.orientation;
    // Looking straight up or down leaves no heading: keep the previous
    // anchor's yaw with the new position, as a game recenter does (09 §5.5).
    const float fx = 2.0f * (q.w * q.y + q.x * q.z), fz = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    const bool vertical = std::sqrt(fx * fx + fz * fz) < 1e-4f;
    const float yaw = vertical ? g.anchor_yaw : std::atan2(fx, fz);
    const XrPosef origin{{0, std::sin(yaw * 0.5f), 0, std::cos(yaw * 0.5f)},
                         {head.position.x, head.position.y, head.position.z}};
    XrSpace s = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, origin);
    if (s == XR_NULL_HANDLE) return false;
    if (g.seated != XR_NULL_HANDLE) xrDestroySpace(g.seated);
    g.seated = s;
    g.anchor_yaw = yaw;
    diag::logf("openxr: recentered (yaw {:.1f} deg{})", yaw * 57.29578f,
               vertical ? ", kept: looking straight up or down" : "");
    return true;
}

const char* state_name(XrSessionState s) noexcept {
    switch (s) {
#define MVR_XR_STATE_CASE(name, value) case name: return #name;
        XR_LIST_ENUM_XrSessionState(MVR_XR_STATE_CASE)
#undef MVR_XR_STATE_CASE
        default: return "unknown XrSessionState";
    }
}

// A running session that the runtime never makes visible is not shown in
// the headset: with SteamVR, usually its Desktop Game Theatre showing the
// game as a flat window. Said once, this long after the session started.
constexpr auto kVisibleTimeout = std::chrono::seconds(10);
std::chrono::steady_clock::time_point g_running_since{};
bool g_visible_once = false;

void check_visible() noexcept {
    if (g_visible_once || g_running_since == std::chrono::steady_clock::time_point{}) return;
    if (g.state == XR_SESSION_STATE_VISIBLE || g.state == XR_SESSION_STATE_FOCUSED) {
        g_visible_once = true;
        return;
    }
    if (std::chrono::steady_clock::now() - g_running_since < kVisibleTimeout) return;
    g_visible_once = true;  // said once
    diag::logf("openxr: session not visible after {} s (state {})", kVisibleTimeout.count(), state_name(g.state));
    integration::show_message_box_async(
        "The OpenXR runtime is not showing the game in the headset.\n\n"
        "With SteamVR this is usually its Desktop Game Theatre. In Steam, open the Properties "
        "of Alien: Isolation and uncheck 'Use Desktop Game Theatre while SteamVR is active', "
        "then restart SteamVR and the game.",
        MB_OK | MB_ICONWARNING);
}

void handle_state(XrSessionState s) {
    g.state = s;
    diag::logf("openxr: session state {}", state_name(s));
    switch (s) {
        case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
            bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            g.running = check(xrBeginSession(g.session, &bi), "xrBeginSession");
            if (g.running) {
                diag::log_line("openxr: session running");
                g_running_since = std::chrono::steady_clock::now();
            }
            break;
        }
        case XR_SESSION_STATE_STOPPING:
            g.running = false;
            check(xrEndSession(g.session), "xrEndSession");
            diag::log_line("openxr: session stopped");
            break;
        case XR_SESSION_STATE_EXITING:
            g.exit_requested = true; // the player quit from the runtime's menu
            g.running = false;
            g.lost = true;
            diag::log_line("openxr: runtime requested exit; game asked to quit, VR off until restart");
            break;
        case XR_SESSION_STATE_LOSS_PENDING:
            // Not a request to quit: the game keeps running on the mirror (09 §3, §7).
            g.running = false;
            g.lost = true;
            diag::log_line("openxr: session loss pending; VR off until restart, the game continues");
            break;
        default:
            break;
    }
}

void poll_events() {
    for (;;) {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        if (xrPollEvent(g.instance, &ev) != XR_SUCCESS) break;
        switch (ev.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                handle_state(reinterpret_cast<const XrEventDataSessionStateChanged&>(ev).state);
                break;
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                g.running = false;
                g.lost = true;
                diag::log_line("openxr: instance loss pending");
                break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                input::on_interaction_profile_changed();
                break;
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                g.recenter_pending = true;
                break;
            default:
                break;
        }
    }
}

void teardown() {
    for (auto& sc : g.eye_chain) destroy_chain(sc);
    destroy_chain(g.screen_chain);
    input::destroy();
    for (XrSpace* s : {&g.seated, &g.local, &g.view}) {
        if (*s != XR_NULL_HANDLE) xrDestroySpace(*s);
        *s = XR_NULL_HANDLE;
    }
    if (g.session != XR_NULL_HANDLE) xrDestroySession(g.session);
    if (g.instance != XR_NULL_HANDLE) xrDestroyInstance(g.instance);
    if (g.context) g.context->Release();
    if (g.device) g.device->Release();
    g = State{};
    g_views.store({});
}

} // namespace

std::filesystem::path probe() noexcept {
    try {
        // An explicit override wins, as with the loader itself.
        wchar_t* env = nullptr;
        if (_wdupenv_s(&env, nullptr, L"XR_RUNTIME_JSON") == 0 && env) {
            const std::filesystem::path set{env};
            std::free(env);
            if (!set.empty()) {
                // The player chose: left as it is, nothing else passed (09 §1).
                g_manifest.clear();
                diag::logf("openxr: available: {} (XR_RUNTIME_JSON set by the player)", diag::utf8(set));
                return set;
            }
        }
        // SteamVR and most runtimes' "make active" write only the 64-bit
        // view, so a different 32-bit entry is usually left over from a
        // runtime used before: prefer the 32-bit build of the one the user
        // made active, and take the 32-bit entry only when that has none.
        const auto m64 = active_runtime(KEY_WOW64_64KEY);
        const auto m32 = active_runtime(KEY_WOW64_32KEY);
        diag::logf("openxr: active runtime 64-bit {}, 32-bit {}",
                   m64.empty() ? "(none)" : diag::utf8(m64), m32.empty() ? "(none)" : diag::utf8(m32));
        std::filesystem::path chosen;
        const char* rule = "";
        if (usable_from_x86(m64)) {
            chosen = m64;
            rule = "the 64-bit registration";
        } else {
            for (const auto& m : x86_siblings(m64)) {
                if (usable_from_x86(m)) {
                    chosen = m;
                    rule = "the 32-bit sibling of the 64-bit registration";
                    break;
                }
            }
        }
        if (chosen.empty() && usable_from_x86(m32)) {
            chosen = m32;
            rule = "the 32-bit registration";
        }
        if (chosen.empty()) {
            if (m64.empty() && m32.empty()) diag::log_line("openxr: not available: no active runtime registered");
            else diag::log_line("openxr: not available: no active runtime has a 32-bit build");
            return {};
        }
        diag::logf("openxr: available: {} ({})", diag::utf8(chosen), rule);
        // Handed to the loader (environment and loader property) only when
        // OpenXR is the runtime that starts: init_system. Probing alone
        // changes nothing in the process (01 §5.2).
        g_manifest = chosen;
        return chosen;
    } catch (const std::exception& e) {
        diag::logf("openxr: probe failed: {}", e.what());
    }
    return {};
}

bool init_system() noexcept {
    if (g.instance != XR_NULL_HANDLE) return true;

    // Instance with the D3D11 binding, and the per-eye FOV query where the
    // runtime offers it (the FOV is needed before a session can exist).
    set_runtime_environment(g_manifest);
    point_loader_at(g_manifest);
    std::uint32_t n_ext = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &n_ext, nullptr);
    std::vector<XrExtensionProperties> props(n_ext, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, n_ext, &n_ext, props.data());
    const auto offered = [&](const char* name) {
        for (const auto& p : props)
            if (std::strcmp(p.extensionName, name) == 0) return true;
        return false;
    };
    const bool fov_ext   = offered(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
    const bool audio_ext = offered(XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME);
    std::vector<const char*> extensions{XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    if (fov_ext) extensions.push_back(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
    if (audio_ext) extensions.push_back(XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME);
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy_s(ci.applicationInfo.applicationName, "Alien: Isolation (MotherVR)", _TRUNCATE);
    strncpy_s(ci.applicationInfo.engineName, "MotherVR", _TRUNCATE);
    ci.applicationInfo.applicationVersion = 1;
    ci.applicationInfo.engineVersion      = 1;
    ci.applicationInfo.apiVersion         = XR_API_VERSION_1_0;
    ci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    ci.enabledExtensionNames = extensions.data();
    if (!check(xrCreateInstance(&ci, &g.instance), "xrCreateInstance")) {
        teardown();
        return false;
    }
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(g.instance, &ip))) {
        diag::logf("openxr: runtime {} {}.{}.{}", ip.runtimeName,
                   XR_VERSION_MAJOR(ip.runtimeVersion), XR_VERSION_MINOR(ip.runtimeVersion),
                   XR_VERSION_PATCH(ip.runtimeVersion));
        strncpy_s(g.info.runtime_name, ip.runtimeName, _TRUNCATE);
    }
    PFN_xrGetAudioOutputDeviceGuidOculus get_audio_output = nullptr;
    if (audio_ext &&
        XR_SUCCEEDED(xrGetInstanceProcAddr(g.instance, "xrGetAudioOutputDeviceGuidOculus",
                                           reinterpret_cast<PFN_xrVoidFunction*>(&get_audio_output)))) {
        wchar_t id[XR_MAX_AUDIO_DEVICE_STR_SIZE_OCULUS] = {};
        if (XR_SUCCEEDED(get_audio_output(g.instance, id))) g.info.audio_output = id;
    }

    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!check(xrGetSystem(g.instance, &si, &g.system), "xrGetSystem (is the headset on?)")) {
        teardown();
        return false;
    }
    XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES};
    if (XR_SUCCEEDED(xrGetSystemProperties(g.instance, g.system, &sp))) {
        strncpy_s(g.info.system_name, sp.systemName, _TRUNCATE);
    }

    std::array<XrViewConfigurationViewFovEPIC, 2> fov{{{XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC},
                                                       {XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC}}};
    g.config_views.fill({XR_TYPE_VIEW_CONFIGURATION_VIEW});
    if (fov_ext) {
        g.config_views[0].next = &fov[0];
        g.config_views[1].next = &fov[1];
    }
    std::uint32_t n = 0;
    if (!check(xrEnumerateViewConfigurationViews(g.instance, g.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                 2, &n, g.config_views.data()),
               "xrEnumerateViewConfigurationViews")) {
        teardown();
        return false;
    }
    g.config_views[0].next = g.config_views[1].next = nullptr;
    g.info.recommended_w = g.config_views[0].recommendedImageRectWidth;
    g.info.recommended_h = g.config_views[0].recommendedImageRectHeight;
    g.info.has_fov = fov_ext;
    if (fov_ext) g.info.fov = {fov[0].recommendedFov, fov[1].recommendedFov};
    diag::logf("openxr: {} per eye {}x{}, FOV {}", g.info.system_name, g.info.recommended_w,
               g.info.recommended_h, fov_ext ? "reported" : "not reported (from the first frame)");
    return true;
}

const SystemInfo& system_info() noexcept { return g.info; }

session_info init(ID3D11Device* device) noexcept {
    session_info r{};
    if (!device || !init_system()) return r;

    // Required before xrCreateSession. The runtime must composite on the
    // adapter the game renders with; warn if they differ (hybrid laptops).
    PFN_xrGetD3D11GraphicsRequirementsKHR get_reqs = nullptr;
    xrGetInstanceProcAddr(g.instance, "xrGetD3D11GraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&get_reqs));
    XrGraphicsRequirementsD3D11KHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    if (!get_reqs || !check(get_reqs(g.instance, g.system, &reqs), "xrGetD3D11GraphicsRequirementsKHR")) {
        teardown();
        return r;
    }
    IDXGIDevice* dxgi = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi)))) {
        IDXGIAdapter* adapter = nullptr;
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&desc)) &&
            (desc.AdapterLuid.LowPart != reqs.adapterLuid.LowPart ||
             desc.AdapterLuid.HighPart != reqs.adapterLuid.HighPart)) {
            // Both adapters, so a hybrid-GPU report can be read from the log (11 §2.4).
            diag::logf("openxr: WARNING game renders on {} (LUID {:08X}:{:08X}), the runtime wants adapter "
                       "LUID {:08X}:{:08X}; continuing",
                       diag::utf8(std::filesystem::path{desc.Description}),
                       static_cast<std::uint32_t>(desc.AdapterLuid.HighPart), desc.AdapterLuid.LowPart,
                       static_cast<std::uint32_t>(reqs.adapterLuid.HighPart), reqs.adapterLuid.LowPart);
        }
        if (adapter) adapter->Release();
        dxgi->Release();
    }

    g.device = device;
    g.device->AddRef();
    g.device->GetImmediateContext(&g.context);

    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = device;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next     = &binding;
    sci.systemId = g.system;
    if (!check(xrCreateSession(g.instance, &sci, &g.session), "xrCreateSession")) {
        teardown();
        return r;
    }

    g.local  = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, kIdentity);
    g.seated = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, kIdentity);
    g.view   = make_reference_space(XR_REFERENCE_SPACE_TYPE_VIEW, kIdentity);
    if (g.local == XR_NULL_HANDLE || g.seated == XR_NULL_HANDLE || g.view == XR_NULL_HANDLE) {
        // Nothing could be located or submitted without them.
        teardown();
        return r;
    }

    std::uint32_t n = 0;
    xrEnumerateEnvironmentBlendModes(g.instance, g.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 1, &n, &g.blend);

    xrEnumerateSwapchainFormats(g.session, 0, &n, nullptr);
    g.formats.resize(n);
    xrEnumerateSwapchainFormats(g.session, n, &n, g.formats.data());

    if (!input::create(g.instance, g.session)) {
        diag::log_line("openxr: controller input unavailable");
    }

    r.ok            = true;
    r.recommended_w = g.config_views[0].recommendedImageRectWidth;
    r.recommended_h = g.config_views[0].recommendedImageRectHeight;
    diag::logf("openxr: session created, per-eye {}x{}", r.recommended_w, r.recommended_h);
    return r;
}

void shutdown() noexcept {
    if (g.session != XR_NULL_HANDLE && g.running) xrEndSession(g.session);
    teardown();
}

bool running() noexcept { return g.running && !g.lost; }

bool exit_requested() noexcept { return g.exit_requested; }

FrameViews views() noexcept { return g_views.load(); }

void set_eye_images(const EyeImages& images) noexcept {
    g.eye_images = images;
    g.eye_images_set = true;
}

void clear_eye_images() noexcept { g.eye_images_set = false; }

void set_screen_image(ID3D11Texture2D* texture, const D3D11_BOX& region) noexcept {
    g.screen_image  = texture;
    g.screen_region = region;
}

void set_screen_pose(const Pose& pose, float width_m) noexcept {
    g.screen_pose     = pose;
    g.screen_width    = width_m;
    g.screen_pose_set = true;
}

void recenter() noexcept { g.recenter_pending = true; }

namespace {

// Second half of a frame: submit what the game rendered for the frame begun
// by the previous frame() and end it, at that frame's own display time:
// copies, Flush, then xrEndFrame (dumps/12_perf_review.md, P7).
void end_frame(IDXGISwapChain* swapchain) {
    g.frame_begun = false;
    const XrTime t = g.frame_time;

    // Layers.
    XrCompositionLayerProjection proj{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::array<XrCompositionLayerProjectionView, 2> pv{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                                                        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const XrCompositionLayerBaseHeader* layer = nullptr;

    if (g.should_render && g.views.valid) {
        if (g.eye_images_set) {
            bool ok = true;
            for (int i = 0; i < 2 && ok; ++i) {
                ID3D11Texture2D* tex = g.eye_images.texture[i];
                if (!tex) { ok = false; break; }
                D3D11_TEXTURE2D_DESC d{};
                tex->GetDesc(&d);
                const D3D11_BOX& box = g.eye_images.region[i];
                ok = ensure_chain(g.eye_chain[i], d.Format, box.right - box.left, box.bottom - box.top) &&
                     copy_into(g.eye_chain[i], tex, box);
                pv[i].pose = to_xr(g.eye_images.view[i].pose);
                pv[i].fov  = {g.eye_images.view[i].angle_left, g.eye_images.view[i].angle_right,
                              g.eye_images.view[i].angle_up, g.eye_images.view[i].angle_down};
                pv[i].subImage.swapchain = g.eye_chain[i].handle;
                pv[i].subImage.imageRect = {{0, 0}, {static_cast<std::int32_t>(g.eye_chain[i].width),
                                                     static_cast<std::int32_t>(g.eye_chain[i].height)}};
            }
            if (ok) {
                proj.space     = g.seated;
                proj.viewCount = 2;
                proj.views     = pv.data();
                layer = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj);
                count_pose_age(g.eye_images.rendered_for);
            }
        } else {
            // Virtual screen: a region of the given image, or else the
            // finished back buffer, on a world-locked quad.
            ID3D11Texture2D* src = g.screen_image;
            D3D11_BOX box = g.screen_region;
            ID3D11Texture2D* back = nullptr;
            if (!src && swapchain &&
                SUCCEEDED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) {
                D3D11_TEXTURE2D_DESC d{};
                back->GetDesc(&d);
                src = back;
                box = {0, 0, 0, d.Width, d.Height, 1};
            }
            if (src) {
                D3D11_TEXTURE2D_DESC d{};
                src->GetDesc(&d);
                const std::uint32_t w = box.right - box.left, h = box.bottom - box.top;
                if (d.SampleDesc.Count == 1 && w && h && ensure_chain(g.screen_chain, d.Format, w, h) &&
                    copy_into(g.screen_chain, src, box)) {
                    const float width = g.screen_pose_set ? g.screen_width : kDefaultScreenWidth;
                    quad.space         = g.seated;
                    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    quad.subImage.swapchain = g.screen_chain.handle;
                    quad.subImage.imageRect = {{0, 0}, {static_cast<std::int32_t>(w), static_cast<std::int32_t>(h)}};
                    quad.pose = g.screen_pose_set ? to_xr(g.screen_pose)
                                                  : XrPosef{{0, 0, 0, 1}, {0, 0, -kDefaultScreenDistance}};
                    quad.size = {width, width * static_cast<float>(h) / static_cast<float>(w)};
                    layer = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
                }
            }
            if (back) back->Release();
        }
    }

    // Start the GPU on the copies now: the runtime's xrEndFrame waits for
    // them, and the immediate context would otherwise hold them until the
    // next flush.
    if (layer) g.context->Flush();

    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
    ei.displayTime          = t;
    ei.environmentBlendMode = g.blend;
    ei.layerCount           = layer ? 1 : 0;
    ei.layers               = layer ? &layer : nullptr;
    check_frame(xrEndFrame(g.session, &ei), "xrEndFrame", g.end_result);
}

// First half of a frame: wait for the runtime to want it, begin it, and
// locate the views and the controllers at its predicted display time.
bool begin_frame() {
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    if (!check_frame(xrWaitFrame(g.session, &wi, &fs), "xrWaitFrame", g.wait_result)) return false;
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    if (!check_frame(xrBeginFrame(g.session, &bi), "xrBeginFrame", g.begin_result)) return false;
    const XrTime t = fs.predictedDisplayTime;
    g.frame_begun   = true;
    g.should_render = fs.shouldRender != XR_FALSE;
    g.frame_time    = t;
    g.frame_period  = fs.predictedDisplayPeriod;

    if (g.recenter_pending && do_recenter(t)) g.recenter_pending = false;

    // Controllers at the start of the frame, so the game's next simulation
    // step reads them (P10). Their grip poses are located at the same time
    // in the same space as the views, and published with them.
    input::sync(t, g.seated);
    for (int h = 0; h < 2; ++h) {
        const auto& s = input::state().hands[h];
        g.views.hands[h] = {s.grip, s.grip_velocity, s.grip_valid, s.grip_valid && s.grip_velocity_valid};
    }

    // Views for this frame, in the seated space.
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = t;
    li.space       = g.seated;
    XrViewState vs{XR_TYPE_VIEW_STATE};
    std::array<XrView, 2> xv{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::uint32_t nv = 0;
    const bool located = XR_SUCCEEDED(xrLocateViews(g.session, &li, &vs, 2, &nv, xv.data())) && nv == 2 &&
                         all_finite(from_xr(xv[0].pose)) && all_finite(from_xr(xv[1].pose));
    // Invalid-pose rules (09 §5.5). Views without a valid orientation are
    // not valid (no projection layer, no FOV, eye offsets kept) and keep
    // the previous values rather than whatever the runtime wrote. Views
    // whose positions alone are invalid stay valid with each eye at its
    // last valid position, matching the held head below.
    g.views.valid = located && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    g.views.eye_positions_valid = g.views.valid && (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT);
    if (g.views.valid) {
        for (int i = 0; i < 2; ++i) {
            g.views.eyes[i] = {from_xr(xv[i].pose), xv[i].fov.angleLeft, xv[i].fov.angleRight,
                               xv[i].fov.angleUp, xv[i].fov.angleDown};
            if (g.views.eye_positions_valid) g.last_eye_pos[i] = g.views.eyes[i].pose.position;
            else g.views.eyes[i].pose.position = g.last_eye_pos[i];
        }
    }
    // The head: without a valid orientation the last head stays whole; with
    // the orientation alone, the new orientation at the last valid position.
    Pose head{};
    const XrSpaceLocationFlags hf = locate_flags(g.view, g.seated, t, head);
    if (hf & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
        g.last_head.orientation = head.orientation;
        if (hf & XR_SPACE_LOCATION_POSITION_VALID_BIT) g.last_head.position = head.position;
    }
    g.views.head = g.last_head;
    g.views.head_valid = (hf & kOrientationAndPosition) == kOrientationAndPosition;
    g.views.display_time = t;
    g_views.store(g.views);
    if (g.views.eye_positions_valid && !g.ipd_logged) {
        const Vec3 a = g.views.eyes[0].pose.position, b = g.views.eyes[1].pose.position;
        diag::logf("openxr: IPD {:.4f} m", std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) +
                                                     (b.z - a.z) * (b.z - a.z)));
        g.ipd_logged = true;
    }
    return true;
}

} // namespace

bool frame(IDXGISwapChain* swapchain) noexcept {
    if (g.session == XR_NULL_HANDLE) return false;
    if (g.frame_begun) end_frame(swapchain);
    if (g.lost) return false;
    poll_events();
    if (!g.running) return false;
    check_visible();
    return begin_frame();
}

} // namespace mothervr::runtime::openxr
