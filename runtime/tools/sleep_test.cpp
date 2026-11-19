#include "platform/sleep.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

using Clock = std::chrono::steady_clock;
// strict: the sleep must never wake before its deadline (host::sleep_until, which the game uses). The standard
// library's is only a comparison: under Wine its sleep_until can return ~0.8 ms early, reported, not failed.
template<class Sleep> void measure(const char* label, Sleep sleep, bool strict) {
    std::array<double, 80> late{};
    double total = 0;
    for (auto& ms : late) {
        auto deadline = Clock::now() + std::chrono::milliseconds(5);
        sleep(deadline);
        auto woke = Clock::now();
        if (woke < deadline) {
            std::printf("%s woke %.3f ms before its deadline\n", label,
                        std::chrono::duration<double, std::milli>(deadline - woke).count());
            assert(!strict);
            woke = deadline;  // counted as on time in the comparison
        }
        ms = std::chrono::duration<double, std::milli>(woke - deadline).count();
        total += ms;
    }
    std::sort(late.begin(), late.end());
    std::printf("%s: 5 ms deadline overshoot mean %.3f ms, p95 %.3f ms, max %.3f ms\n",
                label, total / late.size(), late[75], late.back());
}
int main() {
#ifdef _WIN32
    // Match main.cpp's timer request when comparing the two implementations.
    timeBeginPeriod(1);
#endif
    host::sleep_until(Clock::now() - std::chrono::seconds(1));
    measure("standard sleep", [](auto deadline) { std::this_thread::sleep_until(deadline); }, false);
    measure("host sleep", [](auto deadline) { host::sleep_until(deadline); }, true);
    std::thread other([] {
        auto deadline = Clock::now() + std::chrono::milliseconds(2);
        host::sleep_until(deadline);
        assert(Clock::now() >= deadline);
    });
    other.join();
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}
