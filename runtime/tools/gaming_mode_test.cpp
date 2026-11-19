// Tests platform/gaming_mode.h: which environments are a gamescope session (SteamOS Gaming Mode), and the
// start-up defaults (full screen, the GamePad picture in the TV window) that apply only to unchosen settings.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "platform/gaming_mode.h"
using namespace gaming_mode;
static bool is(const char* got, const char* want) { return got && want ? !strcmp(got, want) : got == want; }
int main() {
    // SteamOS Gaming Mode as a native Linux game sees it
    assert(is(detect(Platform::Linux, {"gamescope", "gamescope-0", "gamescope-0", nullptr}), "XDG_CURRENT_DESKTOP"));
    assert(is(detect(Platform::Linux, {nullptr, "gamescope-0", nullptr, nullptr}), "WAYLAND_DISPLAY"));
    assert(is(detect(Platform::Linux, {"KDE", "wayland-0", "gamescope-0", nullptr}), "GAMESCOPE_WAYLAND_DISPLAY"));
    // a desktop session (SteamOS Desktop Mode, KDE, GNOME) is not
    assert(is(detect(Platform::Linux, {"KDE", "wayland-0", nullptr, nullptr}), nullptr));
    assert(is(detect(Platform::Linux, {"KDE", "wayland-0", "", nullptr}), nullptr));
    assert(is(detect(Platform::Linux, {}), nullptr));
    // Proton: newer Wine keeps the host desktop as WINE_HOST_XDG_CURRENT_DESKTOP
    assert(is(detect(Platform::Windows, {nullptr, nullptr, nullptr, "gamescope"}), "WINE_HOST_XDG_CURRENT_DESKTOP"));
    assert(is(detect(Platform::Linux, {nullptr, nullptr, nullptr, "gamescope"}), nullptr));
    assert(is(detect(Platform::Other, {"gamescope", "gamescope-0", "gamescope-0", "gamescope"}), nullptr));
    // defaults: gaming changes only unchosen settings
    Defaults d = defaults(true, false, false, false);
    assert(d.fullscreen && d.pip);
    d = defaults(true, false, true, true);  // windowed and a GamePad mode chosen before: kept
    assert(!d.fullscreen && !d.pip);
    d = defaults(false, false, false, false);  // a desktop: windowed, the GamePad mode stays as it is
    assert(!d.fullscreen && !d.pip);
    d = defaults(false, true, true, false);  // full screen as it was left
    assert(d.fullscreen);
    puts("gaming_mode_test: gamescope detection and start-up defaults passed");
}
