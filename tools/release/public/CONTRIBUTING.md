# Contributing to SimGolf Remastered

Forks, fixes, new features, mods and bug reports are all welcome.

## Easiest ways to help

- **Report a bug or suggest an idea:** open an [issue](../../issues). Say what you did, what you saw, and attach a screenshot and your `golf.log` (next to `golf.exe`) if the game misbehaved.
- **Make a mod without building anything:** prices, wages, personalities, difficulty and more live in plain text tables. See [docs/modding](docs/modding) and the examples in `mods-examples` inside the release zip. Share a mod as a pull request into `mods/`, or as its own repo.

## Building the program

You need your own copy of the game, like a player does. The repo never contains game files or decompiled code; your build makes them locally from your own `golf.exe`.

1. Requirements: Windows 10 or 11, Git Bash, Python 3, Node 20+, CMake, Visual Studio 2022 Build Tools (MSVC x86 libraries and a Windows SDK), Ghidra 12.1 with OpenJDK 21. `tools/install_native_tools.sh` downloads the rest (LLVM clang-cl and SDL2, official releases) into a tools folder set in `tools/env.sh`.
2. Put your installed game in `game/` and the v1.02 no-CD exe at `game/golf_nocd.exe` (both are ignored by git).
3. Make the decompiler export from your own exe with Ghidra, as described in [port/docs/building.md](port/docs/building.md) and [port/docs/porting.md](port/docs/porting.md).
4. `bash tools/translate_gen.sh` translates it into C under `build/gen` (never committed, never edited by hand).
5. `bash tools/build_native.sh` builds `build/native/golf.exe`. Run it from the game folder.

## Rules for pull requests

1. **No game files, no decompiler output, no upscaled art** in commits (nothing from `game/`, `build/`, `.pcx`, `.flc`, `.sve`, `golf.bin` and so on). The release tools refuse them anyway.
2. **Change behaviour through overrides**, never by editing generated code: a hand-written function in `port/src` marked `// @override FUN_xxxxxxxx`, or a patch entry in the translator json.
3. **Keep the original reachable:** every change gets a switch in `data/mods.ini` (and an Options row if players will want it), so the 2002 behaviour is always one setting away. Tuning numbers go in `data/`, not in code.
4. **Show it works:** say how you tested it, and add a screenshot or a short clip for anything players will see. If game logic is involved, check the game plays the same with your switch off.
5. One feature or fix per pull request, with a short plain description.

## How pull requests get in

Fork the repo, make a branch, and open a pull request against `main`. The maintainer reviews it, tests it with the full test suite, and brings it into the development line by hand, so your commit may land in a combined release commit. You are credited by name in the CHANGELOG and the release notes of the release that includes it.

## What is planned

See the "Coming next" list in the README. If you want to take one on, open an issue first so work is not done twice.

## Licence

Code is MIT (see LICENSE), like the upstream browser port it builds on. SimGolf itself and its art belong to Firaxis, Electronic Arts and Take-Two; this project ships none of their files.
