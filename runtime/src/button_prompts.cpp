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
