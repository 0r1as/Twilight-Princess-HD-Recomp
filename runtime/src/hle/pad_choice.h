// Which controller TP HD plays with: the GamePad or the Pro Controller its start-up prompt offers. The game
// shows its choice by what it reads: up to the prompt, and with the Pro Controller chosen, it reads both the
// GamePad (VPADRead: its screen and touch) and the Pro Controller (KPADReadEx) once per logic step; with the
// GamePad chosen it stops reading the Pro Controller. step() takes one logic step's read counts and says when
// the choice changed (hle/padscore.cpp follows it). Steps the game reads neither (loading) are no evidence.
#pragma once

namespace padscore {
struct PadChoice {
    static constexpr int kProSteps = 3;       // steps in a row reading the Pro Controller: it is played with
    static constexpr int kGamePadSteps = 15;  // steps reading only the GamePad (half a second): the GamePad is
    enum Change { kNone, kPro, kGamePad };
    int pro = 0, gamepad_only = 0;
    Change step(unsigned pro_reads, unsigned gamepad_reads) {
        if (pro_reads) {
            gamepad_only = 0;
            return ++pro == kProSteps ? kPro : kNone;
        }
        if (!gamepad_reads) return kNone;
        pro = 0;
        return ++gamepad_only == kGamePadSteps ? kGamePad : kNone;
    }
};
}  // namespace padscore
