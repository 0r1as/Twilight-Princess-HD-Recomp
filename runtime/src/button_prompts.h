// Button prompts: the game's A/B/X/Y icons drawn as the Wii U's (Original) or with the Xbox label of the button
// at the same position (Xbox: A shows B, B shows A, X shows Y, Y shows X); the buttons themselves keep their
// positions. See button_prompts.cpp and docs/button-prompts.md.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace button_prompts {
enum Style : int { kOriginal = 0, kXbox, kStyles };
const char* name(int style);
// The style drawn this session. It is fixed by the first HD texture the game loads (the HUD's archive is loaded
// at boot), from TPHD_BUTTON_PROMPTS=original|xbox or the saved choice.
int active();
int saved();  // the choice in the Controls tab (applies at the next start)
void set_saved(int style);

// Message text: an inline button icon is a BMG escape (0x1A, length 5, group 0, type 0x000A = A, 0x000B = B).
// X and Y icons are drawn with textures swapped by name (above); A and B can't be (their text textures differ),
// so with the Xbox style the message archives are served with the A and B escapes exchanged instead.
// swap_message_icons: in a decompressed archive (RARC with BMG files), exchanges them in each DAT1 section;
// returns how many. message_archives: for each res/Msg*/bmgres*.arc of the game not already replaced in `files`,
// writes the swapped copy (Yaz0) into `cache` once and adds it to `files` (mods/content.h replacement paths).
size_t swap_message_icons(std::vector<uint8_t>& archive);
void message_archives(const std::filesystem::path& game, const std::filesystem::path& cache,
                      std::map<std::string, std::string>& files);
}  // namespace button_prompts
