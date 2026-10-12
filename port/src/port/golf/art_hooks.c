// Art pack hooks on the golf side (simgolf mod, task-17): tells hd_art.c which
// game file every cached sprite was cut from, so the presenter can draw the
// 4x upscaled frame in its place. Shared by both builds; without a listener
// (the browser build, art pack off) they behave exactly as the originals.
//
// golf prerenders every FLC frame once (FUN_0043d740, port/docs/flic.md): it
// opens the object (and its Shadow) FLC with FUN_00404120, draws each frame
// into the 480x480 scratch surface 0x5aa778 and cuts a rectangle of it into
// the sprite cache with FUN_00473bf0. Interface and building art is cut the
// same way from PCX files loaded into a surface with FUN_00475840.

#include "golf_protos.h"
#include "port/golf/art_hooks.h"

ArtHooks art_hooks;

// @override FUN_00404120
// Opens FLC `path` into flic slot `slot` (0x24e: the shadow slot of the
// prerender); *frames gets its frames per animation.
int FUN_00404120(LPCSTR param_1, int param_2, uint *param_3) {
  int iVar2 = param_2 * 0x84;
  int iVar1 = FUN_00481f40((void *)(0x4f65c8 + iVar2), param_2 * 0xb, param_1, 0, (undefined4 *)(0x4e9aa0 + param_2 * 0x58), 0);
  if (iVar1 != 0) {
    if (param_3 != (uint *)0x0) *param_3 = 0;
    return 0;
  }
  void *this = (void *)(0x50fc20 + param_2 * 0x44);
  FUN_00482b90(this, 0x4f65c8 + iVar2, 0);
  if (art_hooks.flic_open) art_hooks.flic_open(param_1, param_2);
  if (param_3 != (uint *)0x0) {
    if (*(int *)(0x4f660c + iVar2) != 0) {
      *param_3 = (uint) * (ushort *)(*(int *)(0x4f6620 + iVar2) + 0x62);
      return FUN_00482e10((int)this);
    }
    *param_3 = 0;
  }
  return FUN_00482e10((int)this);
}

// @override FUN_00473bf0
// Cuts (x, y, w, h) of an 8-bit surface into sprite wrapper `this` (trimmed
// to its opaque box when param_7: the offset lands in +0x20/+0x24).
int FUN_00473bf0(void *this, int param_1, int param_2, int param_3, int param_4, int param_5, undefined4 param_6, char param_7) {
  if (param_1 == 0) return 0x10;
  int *piVar1 = *(int **)(param_1 + 4);
  if (piVar1 == (int *)0x0) return 7;
  piVar1 = (int *)ICALLT0(*(code **)(*piVar1 + 0xe4), piVar1);
  if (*piVar1 != 8) return 0;
  int r = FUN_00492000(this, param_1, param_2, param_3, param_4, param_5, param_6, param_7);
  if (art_hooks.cut) art_hooks.cut(this, param_1, param_2, param_3, param_4, param_5);
  return r;
}

// FUN_00475840 (translator patch, tools/translate/golf.json) is loading a PCX.
void art_pcx_load(void *surface, const char *path) {
  if (art_hooks.pcx_load && path) art_hooks.pcx_load(surface, path);
}
