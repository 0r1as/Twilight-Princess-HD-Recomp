// Button prompts: the game's A/B/X/Y icons drawn as the Wii U's (Original) or with the Xbox label of the button
// at the same position (Xbox: A shows B, B shows A, X shows Y, Y shows X); the buttons themselves keep their
// positions. See button_prompts.cpp and docs/button-prompts.md.
#pragma once

namespace button_prompts {
enum Style : int { kOriginal = 0, kXbox, kStyles };
const char* name(int style);
// The style drawn this session. It is fixed by the first HD texture the game loads (the HUD's archive is loaded
// at boot), from TPHD_BUTTON_PROMPTS=original|xbox or the saved choice.
int active();
int saved();  // the choice in the Controls tab (applies at the next start)
void set_saved(int style);
}  // namespace button_prompts
