// hle/pad_choice.h: which controller the game plays with, from what it reads each logic step
#include <cassert>
#include <cstdio>

#include "hle/pad_choice.h"

using padscore::PadChoice;

int main() {
    // the title screen and the prompt: both read every step -> the Pro Controller on the third
    {
        PadChoice c;
        assert(c.step(1, 1) == PadChoice::kNone && c.step(1, 1) == PadChoice::kNone);
        assert(c.step(1, 1) == PadChoice::kPro);
        for (int i = 0; i < 100; i++) assert(c.step(1, 1) == PadChoice::kNone);  // said once
        // the GamePad chosen: the Pro Controller isn't read any more -> the GamePad after 15 steps
        for (int i = 1; i < PadChoice::kGamePadSteps; i++) assert(c.step(0, 1) == PadChoice::kNone);
        assert(c.step(0, 1) == PadChoice::kGamePad);
        for (int i = 0; i < 100; i++) assert(c.step(0, 1) == PadChoice::kNone);
    }
    // a lone Pro Controller read amid GamePad reads changes nothing; steps without reads (loading) don't count
    {
        PadChoice c;
        for (int i = 0; i < 10; i++) assert(c.step(0, 1) == PadChoice::kNone);
        assert(c.step(1, 1) == PadChoice::kNone);                                // one read: no switch
        for (int i = 0; i < 50; i++) assert(c.step(0, 0) == PadChoice::kNone);  // loading
        for (int i = 1; i < PadChoice::kGamePadSteps; i++) assert(c.step(0, 1) == PadChoice::kNone);
        assert(c.step(0, 1) == PadChoice::kGamePad);  // counted again from the lone read
    }
    // several reads in one step count as one step
    {
        PadChoice c;
        assert(c.step(4, 1) == PadChoice::kNone && c.step(2, 1) == PadChoice::kNone && c.step(3, 2) == PadChoice::kPro);
    }
    puts("pad_choice: ok");
    return 0;
}
