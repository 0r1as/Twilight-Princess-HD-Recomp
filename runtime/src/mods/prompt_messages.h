// Xbox button prompts in message text. A message shows a button inline with a BMG escape: 0x1A, length 5,
// group 0, type 0x000A = A, 0x000B = B, 0x000F = X, 0x0010 = Y. Their icons can't be relabelled as textures (the A
// and B text textures can't trade places, and the X/Y texture swap in button_prompts.cpp doesn't reach message
// text), so with the Xbox style the message archives are served with A <-> B and X <-> Y exchanged instead.
// main.cpp switches it on (enable) from the session's prompt style before the mod manager starts; packages.cpp
// adds the archives to the content replacements.
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
// In a decompressed archive (RARC with BMG files), exchanges the A/B and X/Y escapes in each DAT1 section;
// returns how many.
size_t swap_message_icons(std::vector<uint8_t>& archive);
// For each res/Msg*/bmgres*.arc of the game not already replaced in `files`, writes the swapped copy (Yaz0) into
// `cache` once and adds it to `files` (mods/content.h replacement paths).
void message_archives(const std::filesystem::path& game, const std::filesystem::path& cache,
                      std::map<std::string, std::string>& files);
}  // namespace mods::prompt_messages
