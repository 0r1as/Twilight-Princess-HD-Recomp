// Button prompts (button_prompts.h). TP HD keeps the Wii version's 2D layouts (res/LayoutRevo) and gives each of
// their textures an HD replacement in the layout's .pack.gz, found by a path this function builds from the Wii
// resource name (02B889F4): out = "/tex/" (or "/geo/" for models) + the path after "/res/", formatted with
// "/%s/%s.%s" or "/%s/%s/%s.%s" and the extension "gtx" (or "gmx"). It runs once per texture when an archive is
// mounted, so a texture handed out under another name is the one the game draws wherever it uses it.
//
// The HD prompts (HUD diamond and action button, menus' Confirm/Back, message Next, map, item ring) are the
// tr_{a,b,x,y}_btn_* icons; with the Xbox style each is handed out as its partner, so the button at a position
// shows that position's Xbox letter. tt_zelda_button_a_text and _b_text can't be swapped: the game stops on a
// null pointer reading the other's data (they differ), and no HD prompt seen uses them. Checked on the title
// screen, Ordon Spring and Village, the item ring, map and Collection (docs/button-prompts.md).
//
// Research aid: TPHD_TEX_TRACE=1 logs each distinct HD path built (archive -> HD path).
#include "button_prompts.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

#include <algorithm>
#include <string_view>

#include "overlay/hostui.h"
#include "runtime.h"

namespace button_prompts {
namespace {
// partners have names of the same length, so the HD path is rewritten in place
const char* const kPairs[][2] = {
    {"tt_zelda_button_x_text.bti.gtx", "tt_zelda_button_y_text.bti.gtx"},
    {"tr_a_btn_00.bti.gtx", "tr_b_btn_00.bti.gtx"},
    {"tr_a_btn_01.bti.gtx", "tr_b_btn_03.bti.gtx"},
    {"tr_x_btn_00.bti.gtx", "tr_y_btn_00.bti.gtx"},
    {"tr_x_btn_01.bti.gtx", "tr_y_btn_01.bti.gtx"},
};
int parse(const std::string& v) { return v == "xbox" || v == "1" ? kXbox : kOriginal; }
int resolve() {
    if (const char* e = getenv("TPHD_BUTTON_PROMPTS")) return parse(e);
    std::string v;
    return hostui::get("buttonPrompts", v) ? parse(v) : kOriginal;
}
std::atomic<int> g_saved{-1};  // -1: not read yet

const char* guest_str(uint32_t ea) { return ea ? (const char*)mem::ptr(ea) : ""; }
void swap_name(uint32_t out) {
    char* path = (char*)mem::ptr(out);
    char* slash = strrchr(path, '/');
    char* name = slash ? slash + 1 : path;
    for (const auto& p : kPairs)
        for (int i = 0; i < 2; i++)
            if (!strcmp(name, p[i])) {
                memcpy(name, p[1 - i], strlen(p[1 - i]));  // same length: the terminator stays
                return;
            }
}
}  // namespace

const char* name(int style) { return style == kXbox ? "Xbox" : "Original"; }
int active() {
    static const int style = [] {
        const int s = resolve();
        LOG("[prompts] button prompts: %s", name(s));
        return s;
    }();
    return style;
}
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
    LOG("[prompts] Xbox prompts in messages: %zu message archives served with A and B swapped (%zu made now, %zu icons)",
        served, made, icons);
}

int saved() {
    int s = g_saved;
    if (s < 0) g_saved = s = resolve();
    return s;
}
void set_saved(int style) {
    g_saved = style;
    hostui::set("buttonPrompts", style == kXbox ? "xbox" : "original");
}
}  // namespace button_prompts

extern "C" void f_02B889F4_orig(Cpu* c);
extern "C" void hook_02B889F4(Cpu* c) {  // (char* out, const char* archive_path, entry names...): the HD path
    using namespace button_prompts;
    static const bool trace = getenv("TPHD_TEX_TRACE") != nullptr;
    const uint32_t out = c->r[3];
    const std::string src = trace ? guest_str(c->r[4]) : std::string();
    f_02B889F4_orig(c);
    if (active() == kXbox) swap_name(out);
    if (!trace) return;
    static std::mutex mu;
    static std::unordered_set<std::string> seen;
    std::lock_guard<std::mutex> lk(mu);
    if (seen.insert(guest_str(out)).second) LOG("[prompts] %s -> %s", src.c_str(), guest_str(out));
}
