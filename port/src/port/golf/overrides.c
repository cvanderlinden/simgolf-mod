// Hand-written replacements for golf.exe functions, marked with
// `// @override <name>` so tools/translate skips the generated version.
// Signatures must match build/gen/golf/golf_protos.h.

#include "golf_protos.h"
#include "port/runtime.h"

// task-40: golf pushes water elevations into Terrain only for tiles near the camera.
// FUN_00449540 (patch in tools/translate/golf.json) asks here whether to push
// every water tile at load; mods.ini [fixes] water_push=0 gives the original.
#ifndef __EMSCRIPTEN__
#include "mods/mods.h"
int fix_water_push(void) { return mods_get_int("fixes.water_push", 1); }
#else
int fix_water_push(void) { return 1; }
#endif

// task-44: Shift+T ("trees off/on") flips bit 0x400 of the game flags at
// 0x59e7b8, which only the terrain trees test. FUN_0040f5c0's decoration
// switch (patches in tools/translate/golf.json) sets g_decor_tree_pass while
// it draws a decoration tree (scenic trees, the cactus group, the scenic-tree
// cell decoration, dead swamp trees) and clears it after the switch; the
// sprite queue functions FUN_004628d0 and FUN_00462a30 drop the sprite then
// when hide_decor_trees() is 1. Only the picture changes: the game's code and
// state (growth frames, cell flags, sounds) run exactly as before, so golfers,
// shots and saves are the same. mods.ini [view] hide_decor_trees=0 gives the
// original Shift+T (terrain trees only). The flag is tested first, so the
// setting is only looked up while trees are hidden.
#define HIDE_TREES_FLAG 0x400u
int g_decor_tree_pass;
#ifndef __EMSCRIPTEN__
int hide_decor_trees(void) {
  if ((*(volatile uint32_t *)(uintptr_t)0x59e7b8 & HIDE_TREES_FLAG) == 0) return 0;
  return mods_get_int("view.hide_decor_trees", 1) != 0;
}
#else
int hide_decor_trees(void) { return (*(volatile uint32_t *)(uintptr_t)0x59e7b8 & HIDE_TREES_FLAG) != 0; }
#endif

// task-48: the original lets a lot be placed on top of a building of its own kind (the
// validity check FUN_0040db90 accepts a tile occupied by the same kind, which the
// upgrade relies on: placing a lot whose level is above 0 first removes the old ones).
// At level 0 nothing is removed, so a click on a built snack bar (or any lot already
// standing) put a second, overlapping one on top and took its price. fix_place_check
// is what the main loop calls for the player's placement click and the hover ghost
// (translator patch in FUN_0040f5c0); the AI's own placements still call FUN_0040db90.
// mods.ini [fixes] building_overlap=0 gives the original (the browser build: always 1).
static int g_overlap_guard;
#ifndef __EMSCRIPTEN__
static int overlap_fix_on(void) { return mods_get_int("fixes.building_overlap", 1); }
#else
static int overlap_fix_on(void) { return 1; }
#endif
int fix_overlap_blocks(int tool) { return g_overlap_guard && tool >= 6 && tool <= 14; }
int fix_place_check(int x, int y, int size, int tool) {
  int level = tool >= 6 && tool <= 14 ? *(volatile int32_t *)(uintptr_t)(0x5a8c38 + tool * 4) : 1;
  g_overlap_guard = level == 0 && overlap_fix_on();
  int r = FUN_0040db90(x, y, size, tool);
  g_overlap_guard = 0;
  return r;
}

// @override FUN_004ae020
// Writes a JPEG snapshot with the statically linked IJG libjpeg 6b
// (jpeg_CreateCompress etc. at 0x4ae020-0x4b6400). Not ported yet: report
// failure, which the snapshot code handles.
undefined4 FUN_004ae020(int param_1, int param_2, int param_3, code *param_4) {
  (void)param_1; (void)param_2; (void)param_3; (void)param_4;
  return 1;
}

// 0x004321a0 copies a string into the global at 0x587da0 (strcpy inlined as
// repne scasb / rep movsd). Ghidra merged it into FUN_00432170 because of
// junk bytes before it, so callers see an undefined func_0x004321a0; the
// return type is int because those call sites have no prototype.
int func_0x004321a0(char *s) {
  char *d = (char *)0x587da0;
  while ((*d++ = *s++) != 0) {}
  return 0;
}

// 0x00474a80 is a C++ static initializer (in the CRT's list at 0x4c1000)
// that Ghidra never made a function: it calls 0x474a90 and tail-jumps to
// 0x474aa0. Registered by golf_register_overrides().
static uint32_t init_00474a80(ICALL_PARAMS) {
  FUN_00474a90();
  FUN_00474aa0();
  return 0;
}

void golf_register_overrides(void) {
  icall_register(0x00474a80, init_00474a80);
}
