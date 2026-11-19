// Xbox button prompts in message text. A message shows a button inline with a BMG escape: 0x1A, length 5,
// group 0, type 0x000A = A, 0x000B = B. X and Y icons are drawn with textures the texture swap exchanges
// (button_prompts.cpp); A and B can't be, so with the Xbox style the message archives are served with the A and B
// escapes exchanged instead. main.cpp switches it on (enable) from the session's prompt style before the mod
// manager starts; packages.cpp adds the archives to the content replacements.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace mods::prompt_messages {
void enable(bool on);
bool enabled();
// In a decompressed archive (RARC with BMG files), exchanges the A and B escapes in each DAT1 section; returns
// how many.
size_t swap_message_icons(std::vector<uint8_t>& archive);
// For each res/Msg*/bmgres*.arc of the game not already replaced in `files`, writes the swapped copy (Yaz0) into
// `cache` once and adds it to `files` (mods/content.h replacement paths).
void message_archives(const std::filesystem::path& game, const std::filesystem::path& cache,
                      std::map<std::string, std::string>& files);
}  // namespace mods::prompt_messages
