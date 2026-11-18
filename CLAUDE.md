# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A static recompilation of Twilight Princess HD (Wii U, USA Rev 2, `game/code/Zelda.rpx`) into a native app.
PowerPC code is translated to C ahead of time, the Cafe OS libraries it imports are reimplemented natively, and
GX2 is implemented directly on Metal (macOS) and Vulkan (Linux, with the SDL3 host). It is a fork of the Wind
Waker HD recomp (the sibling checkout `../wwhd`), so the executable, CMake project and every `WWHD_*` env var keep
upstream names. `docs/how-it-works.md` is the best architecture overview.

Never commit game data or anything derived from it: `game/`, `build/gen`, keys, save states, shader
caches/templates, logs and frame dumps (see `.gitignore`). `tools/release/guard.py` checks packages for this.

## Build and test

```sh
python3 tools/recomp/recomp.py game/code/Zelda.rpx build/gen        # PPC -> C (needs the real game)
cmake -S . -B build/cmake -DWWHD_RENDERER=METAL                      # BOTH (default on macOS) | METAL | VULKAN
make -C build/cmake -j$(sysctl -n hw.ncpu) wwhd
./build/cmake/wwhd                                                   # from the repo root; --game DIR, --save DIR
```

- Without the game: `python3 tools/recomp/stubgen.py build/gen-stub` and configure with `-DGEN_DIR=$PWD/build/gen-stub`
  (builds and runs the unit tests, cannot run the game).
- Clang is required (`musttail`). Non-Apple configs default to `WWHD_RENDERER=VULKAN` with the SDL3 host.
- Linux (README "Build on Linux"; Vulkan + SDL3 host, checked on Arch x86-64 with Clang 23):

  ```sh
  python3 tools/recomp/stubgen.py build/gen-stub                     # placeholder build: wwhd-extract and the tests
  cmake -S . -B build/linux -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DGEN_DIR=$PWD/build/gen-stub
  cmake --build build/linux -j$(nproc) && ctest --test-dir build/linux
  ./build/linux/wwhd-extract --title 000500001019e500 --progress extract GAME.wua game   # a Cemu archive, no keys
  python3 tools/recomp/recomp.py game/code/Zelda.rpx build/gen
  cmake -S . -B build/linux-game -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
  cmake --build build/linux-game -j$(nproc) --target wwhd && ./build/linux-game/wwhd
  # renderer check without the game (lavapipe + validation):
  VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json WWHD_VK_VALIDATION=1 WWHD_HIDDEN_WINDOWS=1 ./build/linux/wwhd --renderer-smoke
  ```
- Tests are CTest targets in `CMakeLists.txt` (sources in `runtime/tools/*_test.cpp`, `tools/wudextract/*_test.cpp`):
  build them, then `ctest --test-dir build/cmake` or one with `ctest --test-dir build/cmake -R <name>` (e.g. `write_watch`).
- Installer helpers: `python3 tools/installer/test_setup.py`.
- In-game checks: `tools/audit/run_route.sh <route.txt> <seconds> [shot times...]` replays scripted GamePad input
  (`tools/audit/routes/*.txt`) and dumps screenshots to `build/audit/<route>/` (macOS and Linux; `APP` defaults to
  the first of `build/cmake-tphd`, `build/linux-game`, `build/cmake` with a build). Headless game checks:
  `WWHD_HIDDEN_WINDOWS=1 WWHD_EXIT_AT_FRAME=<n>` with a scratch `--save` folder. Related: `WWHD_WARP=<frame>:<stage>,<point>,<room>,<layer>` (`tools/warplist.py`
  lists places), `WWHD_STATE_SAVE_AT` / `LOAD=<slot>` for save states, `tools/audit/qasaves.sh`, `stages.sh`, `bosses.sh`.

## Architecture

- **Recompiler** (`tools/recomp/`): `recomp.py` drives `analyze.py` (function discovery, jump tables) and `ppc2c.py`
  (codegen). Every guest function becomes `void f_XXXXXXXX(Cpu* c)` in `build/gen/code_*.c`, plus `table.c` (indirect
  dispatch by guest address) and `imports.c` (weak stubs for Cafe OS imports, overridden by strong runtime
  definitions). Generated code is built as the `gamecode` static lib with `-O3 -ffp-contract=off` for exact FP.
- **Hooks — the main way the runtime changes game behaviour.** `tools/recomp/hooks.txt` (plus any `hooks_*.txt`)
  lists guest addresses. For a function entry `ADDR`, the generated `f_ADDR` calls `hook_ADDR(Cpu*)` and the original
  body is renamed `f_ADDR_orig`; the runtime defines `extern "C" void hook_ADDR(Cpu* c)` and usually calls the orig
  (see `runtime/src/look.cpp`). `@ADDR` is an instruction-level hook: `site_ADDR(c)` runs before that instruction.
  **Adding or removing a hook means re-running `recomp.py`** before rebuilding. Each hooks.txt line has a comment
  naming the TP function (names/layouts come from the zeldaret/tp decompilation) and the runtime file implementing it.
- **Guest memory**: guest address `ea` is at a fixed host base (`PPC_MEM_BASE` in `runtime/include/ppc.h`); access it
  only via the big-endian helpers there (`ld32`, `st8`, `ldf32`, `mem::ptr`, ...). Arguments/returns are `c->r[3..]`,
  `c->f[..]`. Hardcoded guest addresses of game globals/struct offsets are common in hooks and documented in comments.
- **Runtime** (`runtime/src/`): `core.cpp`/`main.cpp` boot and load the RPX; `threads.cpp` maps each guest thread to a
  host thread with a per-core scheduler that reproduces Cafe OS priority/preemption rules (the game depends on this);
  `hle/` is the Cafe OS layer (fs, mem, AX audio, pads, nfp/amiibo, ...).
- **Graphics**: `gx2/` implements the GX2 API into a Latte-compatible register file plus a command queue; a render
  thread turns that into Metal (`gfx/*.mm`) or Vulkan (`gfx/vulkan/`), selected via `gfx/renderer.cpp`. Shaders are
  translated by Cemu's vendored Latte decompiler (`runtime/third_party/cemu`, compiled with `cemu_shim.h`).
  Texture re-upload relies on write-protecting guest pages (`write_watch.h`).
- **Game-specific features built on hooks**: `interp.cpp` (frame interpolation to 60/120/240 fps: logic stays at 30 Hz,
  in-between frames are drawn with camera/models/particles/2D blended, "hold passes" skip logic), `aspect.cpp`
  (wide screen via J2D panes), `look.cpp` (bloom, HUD, fog, sharpness), `peekz.cpp`, `warp.cpp`, `savestate.cpp`.
- **Mods** (`runtime/src/mods/`): built-in mods (`builtin.cpp`, toggled in the overlay's Mods tab and saved in the
  profile) read/write live game state, e.g. `dSv_player_c`; plus content mods (file replacement by `content/` path),
  Cemu graphic packs, and native packages (`runtime/include/wwhd_mod.h`, `docs/mod-manager.md`).
- **UI**: settings overlay is Dear ImGui (`overlay/`), drawn over the game; the macOS menu bar/host is AppKit (`gfx/*.mm`),
  the Linux/Vulkan host is SDL3 (`platform/*_sdl.cpp`, `gfx/vulkan/overlay_sdl.cpp`, settings in `~/.config/tphd/settings.ini`).
  A feature added to the AppKit host (menu item, saved setting, hotkey) usually needs its SDL-host counterpart too.
- **Calling game code from the runtime**: `guest_call(c, addr, {args})` from a guest thread, e.g. the per-step services
  in `interp.cpp`'s `site_02AF0EDC` (`warp::service`, `aspect::service`, `padscore::service`).
- **Other tools**: `tools/installer/setup.py` (first-run setup: extract, verify, recompile, link with prebuilt
  runtime), `tools/release/` (packaging), `tools/wudextract*` (disc extraction), `tools/shaderprep.py` (shader head
  start), `tools/decomp/` (matches RPX functions to a GameCube decomp; built for WWHD, see `docs/decomp-notes.md`).

## Conventions

- Commit messages: `Area: what changed and why` (e.g. `60 fps: ...`, `Mods: ...`, `Aspect: ...`, `Docs: ...`), plain prose.
- New runtime knobs are `WWHD_*` environment variables; user-facing ones are documented in README.md.
