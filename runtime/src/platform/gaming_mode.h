// SteamOS Gaming Mode (and any gamescope session): Steam shows one window at a time, scaled to the screen.
// A game that opens a 1920x1080 window there gets it letterboxed on a 16:10 handheld (Legion Go, Steam
// Deck): the picture can't fill the screen, whatever the aspect setting. In a gamescope session the TV
// window therefore starts full screen (the screen's own size and shape, so "Match window" and 16:10 fill
// it) and the GamePad picture goes into the TV window (picture-in-picture), unless those were chosen in
// the settings before (gfx/vulkan/overlay_sdl.cpp load_saved_options). Ported from upstream (ZeldaWWHDRecomp
// e10e7bd7, gaming_mode.h).
#pragma once
#include <cstdlib>
#include <string_view>

namespace gaming_mode {
enum class Platform { Linux, Windows, Other };
struct Environment {
    const char* desktop = nullptr;            // XDG_CURRENT_DESKTOP ("gamescope" in Gaming Mode)
    const char* wayland = nullptr;            // WAYLAND_DISPLAY (gamescope-0)
    const char* gamescope_wayland = nullptr;  // GAMESCOPE_WAYLAND_DISPLAY (set for games gamescope starts)
    const char* wine_host_desktop = nullptr;  // newer Wine: the host's XDG_CURRENT_DESKTOP (Proton)
};
// Which variable shows a gamescope session, or nullptr. Hardware, Big Picture and compatibility-prefix
// flags do not identify the compositor, so they are not used.
inline const char* detect(Platform platform, Environment env) {
    if (platform == Platform::Other) return nullptr;
    auto contains = [](const char* v) { return v && std::string_view(v).find("gamescope") != std::string_view::npos; };
    if (contains(env.desktop)) return "XDG_CURRENT_DESKTOP";
    if (platform == Platform::Windows && contains(env.wine_host_desktop)) return "WINE_HOST_XDG_CURRENT_DESKTOP";
    if (contains(env.wayland)) return "WAYLAND_DISPLAY";
    if (env.gamescope_wayland && *env.gamescope_wayland) return "GAMESCOPE_WAYLAND_DISPLAY";
    return nullptr;
}
inline const char* reason() {
#if defined(__ANDROID__)
    constexpr auto platform = Platform::Other;
#elif defined(__linux__)
    constexpr auto platform = Platform::Linux;
#elif defined(_WIN32)
    constexpr auto platform = Platform::Windows;
#else
    constexpr auto platform = Platform::Other;
#endif
    return detect(platform, {std::getenv("XDG_CURRENT_DESKTOP"), std::getenv("WAYLAND_DISPLAY"),
                             std::getenv("GAMESCOPE_WAYLAND_DISPLAY"), std::getenv("WINE_HOST_XDG_CURRENT_DESKTOP")});
}
inline bool active() { return reason() != nullptr; }

// The start-up choices: a gaming session changes only what was never chosen (no saved value).
struct Defaults {
    bool fullscreen;
    bool pip;  // the GamePad picture in the TV window
};
inline Defaults defaults(bool gaming, bool fullscreen_saved, bool fullscreen_chosen, bool mode_chosen) {
    return {fullscreen_chosen ? fullscreen_saved : gaming, gaming && !mode_chosen};
}
}  // namespace gaming_mode
