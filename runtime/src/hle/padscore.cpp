// padscore: Wii Remote / Pro Controller (WPAD, KPAD). A Pro Controller is always on channel 0; the game's
// start-up prompt chooses it or the GamePad, and the keyboard and host controllers act as the chosen one
// (service, pad_choice.h). Struct layouts and constants follow Cemu's padscore.
//
// TP HD learns of a controller only through its connect callback (KPADSetConnectCallback, 020086FC for
// channels 0-3): on status 0 it probes the channel (WPADProbe) and, for a Pro Controller, registers it
// with its controller manager (02BFB00C), which then reads it with KPADReadEx; any other status
// unregisters the channel if it was registered. service() reports channel 0 through it once the game has
// registered its callback, and again after a save state is loaded.
#include <atomic>

#include "../crashrec.h"
#include "../overlay/hostui.h"
#include "pad_choice.h"
#include "../runtime.h"
#include "../input.h"
#include "../motion/motion.h"
#include "../rumble.h"

namespace {
constexpr int32_t kWpadErrNone = 0, kWpadErrNoController = -1;
constexpr int32_t kKpadErrNone = 0, kKpadErrNoController = -2;
constexpr uint8_t kDevURCC = 31, kDevNone = 253, kFormatURCC = 22;

// A Pro Controller is always on channel 0, as on a Wii U with one switched on: the game's start-up prompt
// chooses between it and the GamePad, and the keyboard and host controllers act as the chosen one (service).
bool connected(uint32_t chan) { return chan == 0; }
std::atomic<uint32_t> g_kpad_reads{0};  // KPADReadEx calls since the last logic step

// VPAD button bits (input::PadState) -> Pro Controller (URCC) bits
uint32_t pro_buttons(uint32_t v) {
    static const uint32_t map[][2] = {
        {input::kUp, 0x1},     {input::kLeft, 0x2},    {input::kZR, 0x4},     {input::kX, 0x8},
        {input::kA, 0x10},     {input::kY, 0x20},      {input::kB, 0x40},     {input::kZL, 0x80},
        {input::kR, 0x200},    {input::kPlus, 0x400},  {input::kHome, 0x800}, {input::kMinus, 0x1000},
        {input::kL, 0x2000},   {input::kDown, 0x4000}, {input::kRight, 0x8000},
        {input::kStickR, 0x10000}, {input::kStickL, 0x20000},
    };
    uint32_t out = 0;
    for (auto& m : map)
        if (v & m[0]) out |= m[1];
    return out;
}
}  // namespace

HLE(padscore, KPADInit) {}
HLE(padscore, KPADInitEx) {}
HLE(padscore, KPADShutdown) {}
HLE(padscore, KPADEnableDPD) {}
HLE(padscore, KPADDisableDPD) {}
HLE(padscore, KPADEnableMpls) {}
HLE(padscore, KPADDisableMpls) {}
std::atomic<uint32_t> g_connect_cb[4];  // KPADSetConnectCallback, per channel (0: none)
std::atomic<int> g_reported{0};          // channel 0 as last reported: 1 connected, 0 not, -1 unknown

// (chan, callback) -> the previous callback
HLE(padscore, KPADSetConnectCallback) {
    const uint32_t chan = arg(c, 0), cb = arg(c, 1);
    TRACE("[pad] KPADSetConnectCallback(%u, %08X)", chan, cb);
    ret(c, chan < 4 ? g_connect_cb[chan].exchange(cb) : 0);
}

namespace padscore {
std::atomic<uint32_t> g_vpad_reads{0};  // VPADRead calls since the last logic step
void count_vpad_read() { g_vpad_reads++; }
// a guest thread, once per logic step (main01's frame loop, interp.cpp)
void service(Cpu* c) {
    // the game's controller choice (pad_choice.h): the keyboard and host controllers act as that one, and the
    // GamePad screen follows (hostui::set_pro_controller); TPHD_PRO_CONTROLLER (set) keeps the Pro Controller
    static PadChoice choice;
    static const char* fixed = getenv("TPHD_PRO_CONTROLLER");
    const PadChoice::Change change = choice.step(g_kpad_reads.exchange(0), g_vpad_reads.exchange(0));
    if (!fixed && change != PadChoice::kNone && (change == PadChoice::kPro) != input::pro_controller()) {
        const bool pro = change == PadChoice::kPro;
        LOG("[pad] the game plays with the %s: the keyboard and controllers act as it", pro ? "Pro Controller" : "GamePad");
        hostui::post([pro] { hostui::set_pro_controller(pro); });
    }
    const uint32_t cb = g_connect_cb[0];
    if (!cb) return;
    const int now = connected(0) ? 1 : 0;
    int was = g_reported;
    if (was == now || !g_reported.compare_exchange_strong(was, now)) return;
    LOG("[pad] Pro Controller %s (connect callback %08X)", now ? "connected" : "disconnected", cb);
    guest_call(c, cb, {0, now ? (uint32_t)kWpadErrNone : (uint32_t)kWpadErrNoController});
}
// save state loaded: the restored game may know channel 0 either way, so it is reported again
void ss_reset() { g_reported = -1; }
}  // namespace padscore
HLE(padscore, KPADGetMplsWorkSize) { ret(c, 0x5FE0); }
HLE(padscore, KPADSetMplsWorkarea) {}
HLE(padscore, WPADEnableURCC) {}
HLE(padscore, WPADEnableWiiRemote) {}
HLE(padscore, WPADDisconnect) {}
// The Pro Controller motor: cmd 0 stops it, 1 runs it until the game stops it. The Pro
// Controller is only connected while the keyboard/controllers act as one (see connected).
HLE(padscore, WPADControlMotor) {
    uint32_t chan = arg(c, 0), cmd = arg(c, 1);
    if (connected(chan)) {
        TRACE("[pad] WPADControlMotor(%u, %u) -> %s", chan, cmd, cmd ? "rumble" : "stop");
        rumble::pro_motor(chan, cmd != 0);
    }
    ret(c, (uint32_t)kWpadErrNone);
}
HLE(padscore, WPADGetBatteryLevel) { ret(c, 4); }  // full
HLE(padscore, WPADCanSendStreamData) { ret(c, 0); }
HLE(padscore, WPADSendStreamData) { ret(c, (uint32_t)kWpadErrNoController); }
HLE(padscore, WPADControlSpeaker) { ret(c, (uint32_t)kWpadErrNoController); }

HLE(padscore, WPADProbe) {
    // (chan, uint32* type) -> WPAD error
    uint32_t chan = arg(c, 0), type = arg(c, 1);
    bool on = connected(chan);
    TRACE("[pad] WPADProbe(%u) -> %s", chan, on ? "pro" : "none");
    if (type) st32(type, on ? kDevURCC : kDevNone);
    ret(c, (uint32_t)(on ? kWpadErrNone : kWpadErrNoController));
}

HLE(padscore, KPADReadEx) {
    // (chan, KPADStatus* buf, count, int32* err) -> samples written
    uint32_t chan = arg(c, 0), st = arg(c, 1), count = arg(c, 2), err = arg(c, 3);
    TRACE("[pad] KPADReadEx(%u, %08X, %u) connected=%d", chan, st, count, connected(chan));
    if (!connected(chan) || !st || (int32_t)count <= 0) {
        if (err) st32(err, (uint32_t)(connected(chan) ? kKpadErrNone : kKpadErrNoController));
        ret(c, 0);
        return;
    }
    static uint32_t last = 0;
    g_kpad_reads++;
    input::PadState p = crashrec::read(1);  // crash recovery records/replays it
    motion::right_stick(p.rx, p.ry);  // the Pro Controller's stick decides gyro use too (motion.h)
    uint32_t hold = pro_buttons(p.buttons);
    memset(mem::ptr(st), 0, 0xF0);
    st8(st + 0x5C, kDevURCC);     // devType
    st8(st + 0x5D, 0);            // wpadErr
    st8(st + 0x5F, kFormatURCC);  // data_format
    st32(st + 0x60, hold);                // ex_status.uc.hold
    st32(st + 0x64, hold & ~last);        // trig
    st32(st + 0x68, last & ~hold);        // release
    last = hold;
    stf32(st + 0x6C, p.lx); stf32(st + 0x70, p.ly);  // lstick
    stf32(st + 0x74, p.rx); stf32(st + 0x78, p.ry);  // rstick
    st32(st + 0x7C, 1);                   // charge
    st32(st + 0x80, 1);                   // cable
    if (err) st32(err, kKpadErrNone);
    ret(c, 1);
}
