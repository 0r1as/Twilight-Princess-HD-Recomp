# Twilight Princess HD: Recompiled

A static recompilation of The Legend of Zelda: Twilight Princess HD (Wii U, USA disc, Rev 2) into a
native macOS app for Apple Silicon, and (built from source) a native Linux x86-64 program. The game's
PowerPC code (`code/Zelda.rpx`) is translated to C ahead of time, the Cafe OS libraries it imports are
reimplemented natively, and its GX2 graphics are drawn with Metal (macOS) or Vulkan (Linux). There is
no emulator at run time.

It is a fork of [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) (Wind Waker HD),
whose recompiler, Cafe OS layer, Metal renderer, setup and settings overlay it reuses. It is an
unofficial fan project, not affiliated with or endorsed by Nintendo. It is a work in progress; see the
issues for what works and what is open.


Credits for the original project and everything this builds on: [CREDITS.md](CREDITS.md).

| | |
|---|---|
| ![Link in a water cave at night](docs/images/water-cave.png) | ![Link raising his sword](docs/images/sword-raised.png) |

*Screenshots from the game running in this port; the game and its artwork belong to Nintendo.*

## You need your own copy

This repository contains no game code, no game assets and no keys. You need your own, legally
obtained copy of the game, dumped from your own Wii U disc and console. Everything game-specific
(the extracted files, the generated C code, shader caches, save states) is made locally from your
dump and must not be shared.

Supported: the USA disc, title 00050000-1019E500, version 81 (Rev 2). Other regions, the update and
DLC are not; the code is tied to that one `Zelda.rpx` (SHA-256
`28922198517578c9261ac0b0b04a586252033d3bbcf7980b79e37638775b12a9`).

## Status

Work in progress. The game boots through the logos to the title screen, a new file can be started
(name entry, horse name, the opening at the spring) and Ordon Village can be walked around with the
keyboard. Every one of the 78 stages on the disc loads, draws and keeps running when warped to.
Graphics, the HUD and sound work, on macOS (Metal) and on Linux (x86-64, Vulkan). On Linux an Xbox
controller has been played with, as the GamePad and in Pro Controller mode (the game on the TV only).

## Requirements

- A Mac with Apple Silicon, macOS 14 or newer (Metal), or a Linux x86-64 PC with a Vulkan 1.3 driver
  (built from source: [Build on Linux](#build-on-linux)). There are no Windows or Android builds.
- On a Mac: Xcode or Apple's Command Line Tools (`xcode-select --install`), CMake 3.20 or newer.
- Python 3; `tools/wudextract.py` also needs `pycryptodome` (`pip3 install pycryptodome`).
- From your own console and disc: a disc image (`.wux` or `.wud`), its disc key (16 bytes, in a
  `.key` file with the image's name next to it) and the Wii U common key (in `WIIU_COMMON_KEY` as 32
  hex digits, or a `common.key` file next to the image). The setup app (below) also takes a Cemu
  archive (`.wua`) or an already extracted game folder (`code`, `content`, `meta`), which need no
  keys.

## Build from source

```sh
# 1. extract the game into game/ (game.wux, game.key next to it, and the common key)
python3 tools/wudextract.py game.wux extract game
shasum -a 256 game/code/Zelda.rpx      # must match the SHA-256 above

# 2. translate Zelda.rpx to C (writes build/gen; stays on your machine)
python3 tools/recomp/recomp.py game/code/Zelda.rpx build/gen

# 3. build (Metal only)
cmake -S . -B build/cmake -DTPHD_RENDERER=METAL
make -C build/cmake -j$(sysctl -n hw.ncpu) tphd

# 4. play, from the repository root
./build/cmake/tphd
```

`tphd` reads the game from `game/` and writes saves to `save/` in the current directory
(`--game DIR` and `--save DIR` change them). It stops at start if `game/code/Zelda.rpx` does not match
the translated code. (The Wind Waker HD port it is forked from calls these `wwhd` and `WWHD_*`.)

To check that the runtime builds without the game, use placeholder code:
`python3 tools/recomp/stubgen.py build/gen-stub`, then
`cmake -S . -B build/cmake-stub -DTPHD_RENDERER=METAL -DGEN_DIR=$PWD/build/gen-stub && make -C build/cmake-stub tphd`.
The result cannot run the game.

## Build on Linux

Checked on Arch Linux (x86-64, Clang 23, an AMD GPU with RADV). Linux uses the Vulkan renderer and the
SDL3 host: there is no menu bar, so everything is in the settings overlay (F1).

```sh
# Clang, CMake, Python 3, Vulkan headers and loader, SDL 3, glslang, LZ4, zlib, zstd (Arch package names;
# other distributions have their own)
sudo pacman -S --needed clang cmake python vulkan-headers vulkan-icd-loader sdl3 glslang lz4 zlib zstd

# 1. a placeholder build first: it makes the game extractor (tphd-extract) and runs the tests
python3 tools/recomp/stubgen.py build/gen-stub
cmake -S . -B build/linux -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DGEN_DIR=$PWD/build/gen-stub
cmake --build build/linux -j$(nproc)
ctest --test-dir build/linux

# 2. the game into game/: a Cemu archive (.wua, no keys) with tphd-extract, or a disc image with
#    tools/wudextract.py as in "Build from source" (needs pycryptodome and your keys)
./build/linux/tphd-extract --title 000500001019e500 info GAME.wua
./build/linux/tphd-extract --title 000500001019e500 --progress extract GAME.wua game
sha256sum game/code/Zelda.rpx      # must match the SHA-256 above

# 3. translate and build the game (Vulkan is the default off macOS)
python3 tools/recomp/recomp.py game/code/Zelda.rpx build/gen
cmake -S . -B build/linux-game -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/linux-game -j$(nproc) --target tphd

# 4. play, from the repository root
./build/linux-game/tphd
```

Settings, controls, save states and the Vulkan shader cache are kept in `~/.config/tphd` (`$XDG_CONFIG_HOME`).
A portable package of the setup app for Linux is made like the macOS one, with the pinned zig toolchain
(`tools/installer/toolchains.json`), and `python3 tools/release/appimage.py <package folder> --out <dir>` (`--name
TwilightPrincessHD.AppImage` for one file name every build) packs it
as one AppImage (pinned appimagetool and AppImage runtime, SHA-256 checked); the AppImage keeps the game, saves
and settings in `~/.local/share/tphd` and `~/.config/tphd`, or in `<file>.AppImage.home` after
`--appimage-portable-home`.
`--renderer-smoke` checks the Vulkan renderer without the game (`docs/vulkan.md`). The keys differ from a
Mac's where the Mac uses Cmd: Ctrl+P (or F12) captures a frame, Ctrl+G shows or hides the GamePad screen and
F11 or Alt+Enter switches full screen. Save states are not shared between a Mac and Linux (another compression).

**Steam.** The game can be added to Steam as a non-Steam game. Set **Start In** to the repository folder (the
game looks for `game/` and `save/` in the current folder; otherwise use the launch options
`--game DIR --save DIR`). With an Xbox controller, set the shortcut's Steam Input to **Default** (forced on, the
game got no input from it in testing).

**Steam Controller (2026).** Without Steam running, the game reads it directly as a controller (buttons,
sticks, triggers and gyro, checked in play): it turns on SDL's Steam Controller driver, which SDL leaves off
by default; `SDL_JOYSTICK_HIDAPI_STEAM=0` turns it off again. With Steam running, Steam owns the controller:
launch the game from Steam (Steam Input). Steam switches a controller to a game's layout only while it sees the
game's window has the focus, which it can't for a native Wayland window, so a game Steam starts on a Wayland
desktop opens its windows through XWayland (X11); `SDL_VIDEO_DRIVER` set yourself still decides. Checked with a
Steam Controller from a non-Steam-game shortcut.

## Install with the setup app

**Linux (x86-64):** download the `.AppImage` from the
[Releases](https://github.com/0r1as/Twilight-Princess-HD-Recomp/releases) page, make it executable and start it:

```sh
chmod +x TwilightPrincessHD-*-linux-x86_64.AppImage
./TwilightPrincessHD-*-linux-x86_64.AppImage
```

It contains no game code or game files. The first start asks for your game (a disc image and its keys, a
Cemu archive, or an extracted game folder), checks it, extracts it, translates its code and compiles it with
a pinned compiler (zig, downloaded once, about 55 MB): about 4.3 GB of disk and a few minutes. Later starts
open the game directly; `--setup` brings the setup back (repair, another game folder). It needs a Vulkan 1.3
driver and runs on distributions with glibc 2.35 or newer; the setup uses the system's Python 3.8 or newer, or
downloads a pinned copy of Python once (about 75 MB). The game, saves and
settings are kept in `~/.local/share/tphd` and `~/.config/tphd`, or, after `--appimage-portable-home`, in a
`.home` folder next to the AppImage. Without FUSE, start it with `--appimage-extract-and-run`.

**Add to Steam (Linux and Windows):** after the first install the setup asks whether to add the game to Steam
as a non-Steam game, with a cover, wide cover, background, logo and icon (the pictures in `assets/steam`, which
releases carry in `steam/`). The **Add to Steam** button in the setup does it later; `setup.py --add-to-steam`
does it from a terminal. Steam rewrites its shortcuts when it quits, so it has to be closed: the setup offers to
close it and starts it again afterwards. The entry starts the AppImage (or the built game on Windows); an entry
that already starts the same program keeps its name and gets the pictures instead of a second entry. It is added
for the Steam account that signed in last, and the first change keeps a copy of Steam's file as
`shortcuts.vdf.tphd-backup`.

**macOS:** no builds are published. To get the setup app, package the runtime yourself; the package is
built against placeholder code, so it contains no game code:

```sh
python3 tools/recomp/stubgen.py build/gen-stub
cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DTPHD_RENDERER=METAL \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DGEN_DIR=$PWD/build/gen-stub -DTPHD_SETUP_GUI=ON -DTPHD_BUNDLED_ZSTD=ON
cmake --build build/rel
python3 tools/release/package.py --build build/rel --platform macos-arm64 --version 0.1.0 \
  --out build/dist --toolchain xcode-clt --setup-gui build/rel/tphd-setup
python3 tools/release/guard.py build/dist/*.zip    # checks for game code, game files and keys
```

This needs Ninja (`brew install ninja`) and downloads the pinned SDL3 and zstd sources. It writes a
portable folder and a zip to `build/dist/`. In that folder, start **Twilight Princess HD**. The first
start asks for your disc image and keys, a Cemu archive or an extracted game folder, checks that it is
title 00050000-1019E500 version 81 with the expected `Zelda.rpx`, extracts a disc image or archive into
the folder (about 4.3 GB; an extracted folder is used where it is), translates the code to C,
compiles it with Apple's Command Line Tools (it offers Apple's installer if they are missing) and
links it with the prebuilt runtime. Keys are never stored or logged. Later starts launch the game
directly; hold **Shift** while starting it to repair or change the game.

Everything stays in the folder's `data/`: the built game, the extracted game files, saves
(`data/save`), settings, save states and the shader cache (`data/user`) and crash logs
(`data/captures`). The same setup runs in Terminal with `tools/Setup in Terminal.command`. Details:
[tools/installer/README.md](tools/installer/README.md).

## Playing

Two windows open: the TV and the GamePad screen. Click or drag on the GamePad picture to touch it.
Closing the TV window (or Cmd+W, Cmd+Q) quits; closing the GamePad window only hides it.

### Keyboard

| Keyboard | Wii U GamePad |
|---|---|
| W A S D | left stick |
| arrow keys | right stick (camera) |
| K or Space | A |
| J | B |
| L | X |
| I | Y |
| Q / E | L / R |
| Left Shift | ZL |
| C | ZR |
| Return / Tab | + / - |
| H | Home |
| 1 2 3 4 | D-pad up / down / left / right |
| X / V | left / right stick click |

Everything can be remapped in the settings overlay's Controls tab or in **Input > Controls...**,
saved to `~/Library/Application Support/TPHD/controls.json`. Game controllers are read too, with
buttons mapped by position (the bottom face button is the Wii U's B). Keyboard and controllers act
as the Wii U GamePad or as a Pro Controller, whichever you choose in the game's start-up prompt (the Pro
Controller plays on the TV only); until the prompt, they act as the Pro Controller. **Button prompts** in the
Controls tab can draw the game's A/B/X/Y prompts with the Xbox letter of the button at each place (the
Wii U's A shows as B, and so on) from the next start; `TPHD_BUTTON_PROMPTS=original|xbox` for one start
([notes](docs/button-prompts.md)).

App shortcuts in the game window:

| Key | Does |
|---|---|
| F1 (Fn+F1 on most Mac keyboards) or Cmd+, | settings overlay |
| Shift+F2 to Shift+F6 | save state to slot 1 to 5 |
| F2 to F6 | load slot 1 to 5 |
| 6 | 60 fps on / off |
| R | next internal resolution |
| N | 16x anisotropic filtering |
| 8 | edge smoothing (FXAA) |
| Cmd+F | full screen |
| Cmd+G | show / hide the GamePad screen |
| ⌘P or F12 | capture a frame for debugging (to `captures/`) |
| 9 | record 3 s of sound activity for debugging (to `captures/`) |

### Settings overlay

F1 opens an in-game menu over the picture; on a controller hold Select / Minus for half a second or
press Home. Mouse, keyboard and controller work in it; L / R switch tabs, and Esc, F1 or B close it.
The game keeps running but gets no input while it is open. The macOS menu bar (Graphics, Input,
Display, Save States) has most of the same options.

- **Saves**: the five save state slots.
- **Graphics**: frame rate, internal resolution, aspect ratio, 16x anisotropic filtering, FXAA,
  texture sharpness, bloom, the HUD, distant fog, a performance overlay.
- **Display**: full screen, picture scaling (smooth, sharp, integer) and the GamePad screen.
- **Mods**: mod packages and profiles.
- **Controls**: which controller the game plays with (shown), remapping on a drawing of the controller or as a list,
  stick dead zone, inverted camera up/down, Tap amiibo.
- **Language / About**: the text language.

## Features

**Frame rate.** 30 fps as on the Wii U, or 60, 120 or 240 fps by frame interpolation: the game logic
keeps its 30 steps a second, and 1, 3 or 7 frames in between are drawn with the camera, every 3D
model, the particles and the 2D (the HUD's and menus' movements and fades) blended. The rate is capped to the
display's refresh rate, which the Graphics tab shows. `TPHD_INTERP=1` and
`TPHD_INTERP_FPS=60|120|240` set it for one start.

**Aspect ratio.** 16:9 (original), match the window, 16:10, 21:9 or 32:9, through the game's own wide
screen layout: wider screens show more to the sides, text keeps its shape and the HUD keeps its 16:9
distance from the edges. Menus, messages and the title are drawn at 16:9 in the middle, with black beside
full-screen menus such as Collection, the map and the save prompt. The GamePad picture is unchanged.
`TPHD_ASPECT=window|16:10|21:9|32:9`.

**Internal resolution.** 1x (1920x1080, as on the Wii U), 1.33x (2560x1440), 1.5x (2880x1620) or
2x (3840x2160), widened to the aspect ratio (21:9 at 1x is 2520x1080). Measured at 60 fps on a MacBook Air
(M4, fanless): 1x holds 60 at 16:9, 21:9 and 32:9; 1.5x falls to 35-55 as it warms up; 2x to about 30-38.

**Bloom and HUD.** Bloom (the glow around bright areas, strongest in the Twilight) at its original
strength, reduced to half, or off; and the HUD (hearts, rupees, buttons, map icon) shown or hidden, for
screenshots. Menus and messages always show. `TPHD_BLOOM=reduced|off`, `TPHD_HUD=0`.

**Distant fog.** The haze that turns far land toward the sky's color, at its original strength, halved,
or off (as Cemu's TP HD "Distant Fog" pack, which changes the same value). `TPHD_FOG=reduced|off`.

**Texture sharpness.** Original, Sharper or Sharpest: textures seen at a distance or at an angle use
finer detail (a level-of-detail bias of -0.5 or -1, the play settings of Cemu's TP HD pack). Sharpest
can shimmer in motion. Metal and Vulkan (not MoltenVK). `TPHD_SHARPNESS=sharper|sharpest`.

Graphics choices are remembered between starts (`defaults delete tphd` resets them); an environment
variable overrides the saved value for that start only.

**Languages.** The disc carries the text in nine variants: English (North America), English
(Europe), French (North America), French (Europe), Spanish (Latin America), Spanish (Europe),
German, Italian and Japanese. Choose one in Language / About; it applies at the next start. For one
start: `TPHD_LANGUAGE=<code>` (0 Japanese, 1 English, 2 French, 3 German, 4 Italian, 5 Spanish) with
`TPHD_LANGUAGE_REGION=us|eu|jp`.

**Saves.** Saving in the game (Collection, then Save Game) writes one file per Quest Log to
`save/user/`, `ZTP00.dat` for the first, with `COMMON.dat` next to them (`data/save/user/` in a packaged
folder). A save from Cemu or a Wii U (`mlc01/usr/save/00050000/1019e500/user/80000001/`) is imported by the
setup: hold Shift while starting the game, then **Import a save from Cemu or a Wii U** (it offers Cemu's save
when Cemu has one in its default folder), or `tools/Setup in Terminal.command --import-save FOLDER`. A save
already there is moved to `save/user.backup-<time>` first. Copying the files into `save/user/` by hand works
too.

**Save states.** Five slots (keys above, the Saves tab or the Save States menu). A state is the
whole running game, about 190 MB, restored exactly. It contains game code and data: never share it.
States live in `~/Library/Application Support/TPHD/states` (`TPHD_STATE_DIR` changes it) and are
tied to the game file they were made with.

**amiibo.** Virtual figures, no reader or figure files: Input > Tap amiibo (or Tap amiibo... in the
Controls tab) puts Wolf Link, Link, Zelda or Ganondorf on the GamePad's reader until the game reads it.
On the Collection screen, once the game enables amiibo in Faron Woods, Link refills arrows, Zelda
refills hearts, Ganondorf offers to double the damage you take and Wolf Link opens the Cave of Shadows;
on the Quest Log screen Wolf Link stores a Quest Log for quick start, which touching it on the title
screen starts. Wolf Link's data is kept in `~/Library/Application Support/TPHD/amiibo/`.
`TPHD_AMIIBO=<figure>@<frame>` taps one for tests.

**GamePad screen.** A separate window, picture-in-picture in a corner of the TV window (corner, size
and opacity selectable), automatic picture-in-picture (shown for a few seconds when the GamePad
picture changes a lot), off, or the GamePad picture alone in the TV window. Window positions, full
screen and these choices are kept in `~/Library/Application Support/TPHD/display.plist`.

**Mods.** The Mods tab installs mod packages from a folder or ZIP, and Cemu graphic packs made for
TP HD (USA), with profiles. Content mods replace files of the game's `content/` folder by their path
(such as `res/Msgus/bmgres.arc`). Twilight Princess HD has no built-in mods yet, and no package has
been tried with it.

**Testing aids.** `TPHD_WARP=<frame>:<stage>,<point>,<room>,<layer>` warps to any place of the
developer map select, and more warps can follow, separated by `;` (`tools/warplist.py` lists the
1,987 places; Ordon Village on day 1 is `F_SP103,1,0,6`). `TPHD_STATE_SAVE_AT=<frame>:<slot>` takes a save state at a frame. Scripted routes
with screenshots: `tools/audit/run_route.sh`.

**Shader head start.** The first time the game meets a shader it translates and compiles it. A head start
translates the shaders your play has met so far at the next start instead, from the game's own
`content/Shaders.pack.gz` and a template of the states they were drawn with, recorded from your shader cache.
Both are made on your machine and must not be shared; the setup app does not make them. In a source checkout,
after playing for a while:

```sh
python3 tools/shaderprep.py template ~/Library/Caches/tphd/shaders.bin   # -> game/shadercache/template.bin
python3 tools/shaderprep.py build                                          # -> game/shadercache/headstart.bin
```

`--merge game/shadercache/template.bin` adds a later session to the template, and `./build/cmake/tphd
--warm-shaders` compiles everything once to fill the macOS shader cache. The game loads
`game/shadercache/headstart.bin` by itself (`TPHD_HEADSTART=<file>` or `0` changes or disables it).

Settings, controls, mod packages and save states are kept in `~/Library/Application Support/TPHD`,
the shader cache in `~/Library/Caches/tphd` (both in `data/user` in a packaged folder). Crashes write
`captures/crash-<time>.log`.

## Known limitations

- macOS on Apple Silicon (Metal) and Linux x86-64 (Vulkan; the AppImage, or built from source), checked on
  one Linux machine (Arch Linux, an AMD GPU with RADV). No Windows build has been made. The Vulkan renderer
  also runs on a Mac through MoltenVK (`docs/vulkan.md`).
- Linux: F12 doesn't capture while the settings overlay is open (it has the keyboard). Launched from Steam on a
  Wayland desktop, the game's windows go through XWayland (for Steam Input), so desktop scaling may look
  softer there.
- Not yet checked: a real controller, playing whole dungeons and bosses (the disc's QA saves load and play
  from every part of the story: `tools/audit/qasaves.sh`).
- Frame interpolation blends the camera, models (with their texture and color animation), particles,
  the 2D's movements and fades, and the screen fades; screen effects and 2D color and texture
  animations move at 30 Hz, and true 60 fps logic is not available.
- At 32:9 the field map's province view doesn't highlight the region under the cursor.
- amiibo: the Cave of Shadows has been entered with Wolf Link but not played through.
- Warping straight into the Temple of Time's Darknut room (D_MN06B) crashes the GamePad dungeon map;
  reached through the temple, as in play, it works.

## Credits

- [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) and its contributors: the
  recompiler, the Cafe OS layer, the GX2-on-Metal renderer, the setup and the settings overlay this
  port is built on.
- [Cemu](https://github.com/cemu-project/Cemu) (MPL-2.0): the vendored GPU address library and
  shader decompiler and some reference structures (`runtime/third_party/cemu`); `tools/wudextract.py`
  and parts of the OS layer are ported from or follow Cemu, as noted in those files.
- [zeldaret/tp](https://github.com/zeldaret/tp): the Twilight Princess decompilation, the source of
  the function names and structure layouts the hooks use.
- Vendored: [metal-cpp](https://developer.apple.com/metal/cpp/) (Apache-2.0),
  [{fmt}](https://github.com/fmtlib/fmt) (MIT), [Dear ImGui](https://github.com/ocornut/imgui) (MIT)
  and [xxHash](https://github.com/Cyan4973/xxHash) (BSD-2-Clause), each with its license in
  `runtime/third_party/`. The `.wua` reader is written from the format of
  [ZArchive](https://github.com/Exzap/ZArchive) by Exzap; [zstd](https://github.com/facebook/zstd)
  (BSD-3-Clause) decompresses it, and the setup app uses [SDL3](https://github.com/libsdl-org/SDL).

## License

The code of this project is licensed under the Mozilla Public License 2.0 (see `LICENSE`), as
ZeldaWWHDRecomp is. Vendored third-party code keeps its own license. "The Legend of Zelda",
"Twilight Princess" and "Wii U" are trademarks of Nintendo, used here only to describe what this
software works with; the game itself is Nintendo's property and is not included.

## More projects

- [Wind Waker HD: native port + mods](https://github.com/AsadSi/wwhd-recomp-mods): spooky sea events, Tetra's ship, online co-op
- [Ocarina of Time: Recompiled](https://github.com/AsadSi/ocarina-of-time-recomp): native port with mods (unofficial fork)
- [Fixframe](https://github.com/AsadSi/fixframe-debugger): live render debugger for emulators and ports
