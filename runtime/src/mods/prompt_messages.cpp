// Xbox button prompts in message text (prompt_messages.h, docs/button-prompts.md). Self-contained (standard
// library only), like tunic.cpp, so the mod manager and its tests link it on their own.
#include "prompt_messages.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace mods::prompt_messages {
namespace {
std::atomic<bool> g_on{false};
}  // namespace
void enable(bool on) { g_on = on; }
bool enabled() { return g_on; }

namespace {
uint32_t be32(const std::vector<uint8_t>& d, size_t at) {
    return at + 4 <= d.size() ? (uint32_t)d[at] << 24 | d[at + 1] << 16 | d[at + 2] << 8 | d[at + 3] : 0;
}
// Yaz0 (the archives' compression): decode, and encode as literals only (valid Yaz0, about 1/8 larger than the
// data; the archive loader decompresses it as before)
bool yaz0_decode(const std::vector<uint8_t>& src, std::vector<uint8_t>& out) {
    if (src.size() < 16 || memcmp(src.data(), "Yaz0", 4) != 0) return false;
    const size_t size = be32(src, 4);
    out.clear();
    out.reserve(size);
    size_t i = 16;
    while (out.size() < size) {
        if (i >= src.size()) return false;
        const uint8_t code = src[i++];
        for (int b = 0; b < 8 && out.size() < size; b++) {
            if (code & (0x80 >> b)) {
                if (i >= src.size()) return false;
                out.push_back(src[i++]);
                continue;
            }
            if (i + 1 >= src.size()) return false;
            const uint8_t b1 = src[i], b2 = src[i + 1];
            i += 2;
            const size_t dist = ((size_t)(b1 & 0xF) << 8 | b2) + 1;
            size_t n = b1 >> 4;
            if (n == 0) {
                if (i >= src.size()) return false;
                n = src[i++] + 0x12;
            } else {
                n += 2;
            }
            if (dist > out.size()) return false;
            for (size_t k = 0; k < n; k++) out.push_back(out[out.size() - dist]);
        }
    }
    return true;
}
std::vector<uint8_t> yaz0_literals(const std::vector<uint8_t>& d) {
    std::vector<uint8_t> out = {'Y', 'a', 'z', '0', (uint8_t)(d.size() >> 24), (uint8_t)(d.size() >> 16),
                                (uint8_t)(d.size() >> 8), (uint8_t)d.size(), 0, 0, 0, 0, 0, 0, 0, 0};
    out.reserve(16 + d.size() + d.size() / 8 + 1);
    for (size_t i = 0; i < d.size(); i += 8) {
        const size_t n = std::min<size_t>(8, d.size() - i);
        out.push_back((uint8_t)(0xFF00 >> n));  // n literal bits
        out.insert(out.end(), d.begin() + i, d.begin() + i + n);
    }
    return out;
}
}  // namespace

size_t swap_message_icons(std::vector<uint8_t>& d) {
    size_t swapped = 0;
    for (size_t bmg = 0; (bmg = std::string_view((const char*)d.data(), d.size()).find("MESGbmg1", bmg)) != std::string_view::npos;
         bmg += 8) {
        if (bmg + 0x20 > d.size()) break;
        const uint8_t encoding = d[bmg + 0x10];
        if (encoding == 2) continue;  // UTF-16 (none in TP HD's US release): escapes are laid out differently
        const size_t total = be32(d, bmg + 8);
        const size_t end = std::min(d.size(), bmg + total);
        for (size_t sec = bmg + 0x20; sec + 8 <= end;) {
            const size_t len = be32(d, sec + 4);
            if (len < 8) break;
            if (!memcmp(&d[sec], "DAT1", 4)) {
                const size_t stop = std::min(end, sec + len);
                for (size_t i = sec + 8; i < stop;) {
                    if (d[i] != 0x1A || i + 1 >= stop) { i++; continue; }
                    const size_t n = d[i + 1];
                    if (n == 5 && i + 5 <= stop && d[i + 2] == 0 && d[i + 3] == 0 && (d[i + 4] == 0x0A || d[i + 4] == 0x0B)) {
                        d[i + 4] ^= 0x01;  // 0x0A <-> 0x0B
                        swapped++;
                    }
                    i += n >= 2 ? n : 1;
                }
            }
            sec += len;
        }
    }
    return swapped;
}

void message_archives(const std::filesystem::path& game, const std::filesystem::path& cache,
                      std::map<std::string, std::string>& files) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path res = game / "content" / "res";
    if (!fs::is_directory(res, ec)) return;
    size_t made = 0, icons = 0, served = 0;
    for (const auto& dir : fs::directory_iterator(res, ec)) {
        std::string folder = dir.path().filename().string();
        if (!dir.is_directory() || folder.rfind("Msg", 0) != 0) continue;
        for (const auto& f : fs::directory_iterator(dir.path(), ec)) {
            std::string name = f.path().filename().string();
            if (name.rfind("bmgres", 0) != 0 || f.path().extension() != ".arc") continue;
            std::string key = "res/" + folder + "/" + name;  // replacement keys are lower case (content.h)
            for (char& ch : key) ch = (char)tolower((unsigned char)ch);
            if (files.count(key)) continue;  // a content mod's own message file wins
            const fs::path out = cache / "xbox-prompts" / folder / name;
            if (!fs::exists(out, ec) || fs::last_write_time(out, ec) < fs::last_write_time(f.path(), ec)) {
                std::vector<uint8_t> src, arc;
                {
                    FILE* in = fopen(f.path().string().c_str(), "rb");
                    if (!in) continue;
                    uint8_t buf[1 << 16];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof buf, in)) > 0) src.insert(src.end(), buf, buf + n);
                    fclose(in);
                }
                if (!yaz0_decode(src, arc)) arc = src;  // stored uncompressed
                const size_t n = swap_message_icons(arc);
                icons += n;
                const std::vector<uint8_t> packed = yaz0_literals(arc);
                fs::create_directories(out.parent_path(), ec);
                const fs::path tmp = out.string() + ".tmp";
                FILE* o = fopen(tmp.string().c_str(), "wb");
                if (!o) continue;
                const bool ok = fwrite(packed.data(), 1, packed.size(), o) == packed.size();
                fclose(o);
                if (!ok) { fs::remove(tmp, ec); continue; }
                fs::rename(tmp, out, ec);
                if (ec) continue;
                made++;
            }
            files.emplace(key, out.string());
            served++;
        }
    }
    fprintf(stderr, "[prompts] Xbox prompts in messages: %zu message archives served with A and B swapped (%zu made now, %zu icons)\n",
            served, made, icons);
}

}  // namespace mods::prompt_messages
