// The course map screens on bigger maps (task-8): the routing map
// (FUN_00456be0, also the shade and home value views) and the buy land
// screen (FUN_004587a0) draw every tile as a small diamond, 12 x 6 px for a
// 50 x 50 map, through FUN_00456b70. Here the step shrinks with the map so
// any size fills the same part of the screen; the diamonds' half sizes
// (GEN_MAP_HW, GEN_MAP_HH in port/grid.h) shrink with it (golf.json patches).
// At 50 x 50 every value is the original's.

#include "golf_protos.h"
#include "port/grid.h"

#define DAT_00822b80 (*(int *)0x822b80)   // 1 while the buy land screen draws

// @override FUN_00456b70
// Tile (x, y) to the screen point of its diamond; returns the y nudge
// (-1, or 1 on the buy land screen) as the original does.
int FUN_00456b70(int param_1, int param_2, int *param_3, int *param_4) {
  int w = g_map_w;
  int nudge = (DAT_00822b80 != 0) ? 1 : -1;
  *param_3 = (param_1 + param_2) * 300 / w + 0x6a;
  // (y - x) * 150 / w rounded down for negative values too: shift by w first
  *param_4 = ((param_2 - param_1 + w) * 150) / w - 150 - nudge + 0x1b8;
  return nudge;
}
