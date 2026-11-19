# Button prompts

Goal: keep the original button mapping (by position: the Wii U's A is the right face button, Xbox B) but draw
the game's prompts with the Xbox label of the button at that position, letters only:

| Game button | Position | Prompt shows |
|---|---|---|
| A | right | B |
| B | bottom | A |
| X | top | Y |
| Y | left | X |

Wind Waker HD's Button prompts (upstream `button_glyphs.cpp`) only recolour, and its hooks are Wind Waker's,
so none of it carries over.

## Where TP HD's 2D textures come from

TP HD keeps the Wii version's 2D layouts (`content/res/LayoutRevo/*R.arc`, plus some from `res/Layout`) and
gives each texture an HD replacement in the archive's `.pack.gz` (a `TMPK` bundle of GTX textures). The HD path
is built per texture by `02B889F4` (hooked in `runtime/src/button_prompts.cpp`):

    (char* out, const char* archive_path, entry names...) -> "/tex/<archive path after /res/>/<dir>/<name>.bti.gtx"

with the prefix `"/tex/"` (`100AFBF0`; `"/geo/"`, `100AFBF8`, for models), the formats `"/%s/%s.%s"`
(`100AFC00`) / `"/%s/%s/%s.%s"` (`100AFC0C`) and the extension `"gtx"` (`100AFBE8`; `"gmx"`, `100AFBEC`). It runs
once per texture when an archive is mounted, so a texture handed out under another name is the one the game
draws wherever it uses it. `TPHD_TEX_TRACE=1` logs each HD path built.

## Button textures seen (title, Ordon Spring, Ordon Village, menus; 1,608 HD paths)

- **HUD** (`LayoutRevo/main2DR.arc`): `tt_zelda_button_{a,b,x,y}_text`, `tt_zelda_button_{x,y,r}_base`,
  `tt_zelda_button_ab_maru`, `tt_zelda_button_a_8ia`, `im_b_button_01`, `im_back_button`,
  `im_zelda_{+,-}button_text_00`, `tr_{a,b,x,y,l,r,zl,zr}_btn_*` (`a` 00/01, `b` 00/03, `x` 00/01, `y` 00/01),
  `tr_cross_btn`, `tr_rstick_00`, `minmap_buttons`, `tt_zelda_revo_kakubutton_48_8ia_00`,
  `tt_zelda_revo_maru_button_64_8ia_01`.
- **Buttons layout** (`buttonR.arc`): `tt_zelda_button_{a,b}_text`, `_a_8ia`, `_ab_maru`, `im_b_button_01`, Wii
  Remote / Nunchuk art (`tt_revocon_01`, `tt_nunchaku_*`, `im_zelda_button_c_text_*`, `im_z_button_02`).
- **Menus**: Collection (`clctresR`), field and dungeon maps (`fmapresR`, `dmapresR`, `gamepad_*mapresR`),
  item info (`itmInfResR`) reuse `tt_zelda_button_a_8ia`, `tt_zelda_button_b_text`, `im_b_button_01`, and have
  generic `button1`/`button2` (plus `button1b`/`button2b`, `floorbutton*`, `btnl`).
- **Item ring** (`ringresR`): HD icons `btn_a`, `btn_x`, `btn_y`, `btn_r`, `btn_zl`; no `btn_b`.
- **Messages** (`msgres00R`): only `button1`/`button2` and button bases; inline button icons in message text
  are likely glyphs of the font texture (`Fontus/fontres.arc` -> `rodan_b_24_22.bfn.gtx`). Not confirmed.

## What the Xbox style swaps (button_prompts.cpp)

Controls -> Button prompts -> Xbox letters (saved as `buttonPrompts`; `TPHD_BUTTON_PROMPTS=original|xbox` for one
start) hands each of these textures out under its partner's name. The style is fixed when the game loads its
first HD texture (the HUD's archive loads at boot), so a change applies at the next start.

| Pair | Checked |
|---|---|
| `tr_a_btn_00` <-> `tr_b_btn_00`, `tr_a_btn_01` <-> `tr_b_btn_03` | HUD diamond and action button, menus' Confirm/Back, message Next, map zoom, Collection Back |
| `tr_x_btn_00` <-> `tr_y_btn_00`, `tr_x_btn_01` <-> `tr_y_btn_01` | HUD diamond, item ring (diamond and Set item) |
| `tt_zelda_button_x_text` <-> `tt_zelda_button_y_text` | boots and draws; no screen seen uses it |

`tt_zelda_button_a_text` <-> `tt_zelda_button_b_text` is left out: with it the game stops at boot on a null pointer
(guest 02B88C84, reading the swapped texture's data), so the two differ; no HD prompt seen uses them. Checked with
the title screen, Ordon Spring and Village, the item ring, field map and Collection (frame captures, Linux).

## Not covered

- Textures with no partner (`tt_zelda_button_a_8ia`, `im_b_button_01`, the ring's `btn_a`/`btn_x`/`btn_y`) stay
  as they are; none showed up on the screens checked.
- Button icons inside message text, if the game has any: none seen yet (likely font glyphs, `rodan_b_24_22.bfn`).
- Screens not visited yet (fishing, letters, minigames, the GamePad screen's own layouts).

## Icons in message text

A message shows a button inline with a BMG escape: 0x1A, length 5, group 0, type 0x000A (A) or 0x000B (B); the
HD-only icons (ZL, ...) are group 7. X and Y in messages are drawn with the `tt_zelda_button_{x,y}_text` textures,
which the texture swap above already exchanges. A and B can't be swapped as textures, so with the Xbox style the
runtime serves the message archives (`res/Msg*/bmgres*.arc`, every language) with those two escapes exchanged
(`mods/prompt_messages.cpp`, switched on in main.cpp, called by the mod manager before content replacements start). Each archive
is decompressed (Yaz0), only the A/B escapes inside BMG DAT1 sections change (the text is walked escape by escape;
UTF-16 BMGs, which TP HD doesn't ship, are skipped), and the copy is written back as literal-only Yaz0 into the
mod cache (`ModManager/Cache/xbox-prompts/`), once. A content mod that replaces a message archive wins. The style
is fixed for the session, as for the textures: a change in the Controls tab applies at the next start.

Checked: 90 archives, 1926 icons; compared with the originals, every changed byte is an A/B icon code; the game
reads the copies (Msgus at boot) and plays.
