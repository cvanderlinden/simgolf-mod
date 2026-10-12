// Round HD golf ball: the hooks translator patches put around golf's own ball
// draws (tools/translate/golf.ball.json, task-41). See port/hdball.h and
// port/hdball.c for the split. With ui.ball=original (and in the browser
// build) every hook just makes the original call: the same pixels as before.
#include "golf_protos.h"
#include "port/runtime.h"

#define G32(a) (*(volatile int32_t *)(uintptr_t)(a))

#define LINE_SURFACE ((void *)(uintptr_t)0x519cd4)   // &DAT_00519cd4, golf's line/pixel drawing object

#ifdef __EMSCRIPTEN__

void hdball_rest(int slot, int sx, int sy) {
  (void)slot;
  FUN_00478b80(LINE_SURFACE, sx, sy, sx + 1, sy, 0x80007fff);
}
void hdball_air(int slot, int sx, int sy) { (void)slot, (void)sx, (void)sy; }
void hdball_item(int i) { FUN_00462be0(i); }
void hdball_resolve(void) {}
void hdball_trail(int slot, int x0, int y0, int x1, int y1, int col, int w, int a) {
  (void)slot;
  FUN_00406220(x0, y0, x1, y1, col, w, a);
}

#else

#include "port/hdball.h"

// A ball at rest on a hole: golf draws a 2 px white line at the ground point.
void hdball_rest(int slot, int sx, int sy) {
  if (hb_note(slot, HB_KIND_REST, sx, sy)) {
    hb_snap_begin(1, slot, sx - 3, sy - 3, 9, 7);
    FUN_00478b80(LINE_SURFACE, sx, sy, sx + 1, sy, 0x80007fff);
    hb_snap_end();
  } else {
    FUN_00478b80(LINE_SURFACE, sx, sy, sx + 1, sy, 0x80007fff);
  }
}

// A ball in flight or rolling: golf queues a shadow and a ball sprite (drawn
// later by FUN_00463100, through hdball_item). Here only the ball is noted.
void hdball_air(int slot, int sx, int sy) { hb_note(slot, HB_KIND_AIR, sx, sy); }

// golf's trail behind some balls in flight: one segment from the last game frame's ball position to this
// one. With the round ball on it is not drawn (the presenter draws a smooth ribbon, ui.ball_trail), unless
// ui.ball_trail=original.
void hdball_trail(int slot, int x0, int y0, int x1, int y1, int col, int w, int a) {
  if (hb_trail(slot)) return;
  FUN_00406220(x0, y0, x1, y1, col, w, a);
}

#define SPR_BALL 0x58b650u     // &DAT_0058b650: golf's ball sprite
#define SPR_SHADOW 0x5791c8u   // &DAT_005791c8: its shadow

// FUN_00463100's draw of one queued sprite. The ball and shadow sprites of a
// frame the round ball is on are drawn as ever and measured (hdball_resolve
// takes them out again).
void hdball_item(int i) {
  if (hb_frame_on() && (uint32_t)G32(0x82815c + 4 * i) == 0xffffffffu) {
    uint32_t spr = (uint32_t)G32(0x82a15c + 4 * i);
    int kind = spr == SPR_BALL ? 1 : spr == SPR_SHADOW ? 2 : 0;
    if (kind) {
      int rec = hb_match(kind, G32(0x82b160 + 4 * i), G32(0x82415c + 4 * i));
      int ox = G32(0x830164 + 4 * i), oy = G32(0x831164 + 4 * i);
      hb_snap_begin(kind, rec, ox - 3, oy - 3, 12, 12);
      FUN_00462be0(i);
      hb_snap_end();
      return;
    }
  }
  FUN_00462be0(i);
}

#endif
