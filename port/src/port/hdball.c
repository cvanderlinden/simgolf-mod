// Round HD golf ball, game-thread half (task-41). See hdball.h.
//
// golf draws each ball in the golfer pass of its main loop (FUN_0040f5c0):
// a ball at rest on a hole as a 2 px line straight into the back buffer, any
// other (in flight, rolling, off the holes) as a shadow sprite and a ball
// sprite queued in golf's depth-sorted sprite list (FUN_00462a30), which
// FUN_00463100 draws after everything else of the course. With the round ball
// on, translator patches (tools/translate/golf.ball.json) route those draws
// through golf/ball_hooks.c: the original draw still runs, between two
// snapshots of the few pixels it touches. When the sprite pass is done
// (hdball_resolve) every pixel golf's ball changed that nothing drew over
// since goes back to what was under it, and the share that was covered is
// kept as the ball's visibility (a ball behind a tree stays behind it). The
// round ball itself is drawn by the presenter at screen resolution.
//
// Nothing here writes game state: the pixels of the back buffer and the
// published record are the only outputs. With ui.ball=original the hooks call
// the original draws and do nothing else.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mods/mods.h"
#include "port/golf/hd_ui.h"
#include "port/hdball.h"
#include "port/host.h"

#define G32(a) (*(volatile int32_t *)(uintptr_t)(a))

#define BALL_X 0x57958c   // golfer record + 0xd4
#define BALL_Y 0x579590
#define BALL_Z 0x579594
#define ZOOM 0x4c2844
#define SCALE 0x4c2e00
#define ZOOM2 0x5a9cbc    // golf's 2x zoom view

int hd_native_balls_ok(void);   // hd_native.c

typedef struct {
  int kind;          // 0 line (ball at rest), 1 ball sprite, 2 shadow sprite
  int rec;           // slot of the record it belongs to, -1 none
  int x0, y0, w, h;
  uint16_t before[256], after[256];
  int n_changed;
} Pend;

#define MAX_PEND (HB_MAX * 2 + 4)
static struct {
  int decided, on;
  uint32_t frame;
  HdBallRec rec[HB_MAX];
  uint32_t stamp[HB_MAX];
  Pend pend[MAX_PEND];
  int npend, open;   // open: the entry hb_snap_begin made and hb_snap_end has not closed
} g = {0, 0, 1};

static int setting(void) {
  static unsigned seen = ~0u;
  static int on;
  unsigned s = mods_serial();
  if (s != seen) {
    seen = s;
    const char *v = mods_get("ui.ball");
    on = v && !strcasecmp(v, "round");
  }
  return on;
}

int hdball_on(void) { return setting() && hd_native_balls_ok(); }

int hb_frame_on(void) { return g.decided && g.on; }

int hb_note(int slot, int kind, int sx, int sy) {
  if (!g.decided) {
    g.on = hdball_on();
    g.decided = 1;
  }
  if (!g.on || slot < 0 || slot >= HB_MAX) return 0;
  HdBallRec *r = &g.rec[slot];
  int z = G32(BALL_Z + slot * 0x100);
  int t = (G32(SCALE) * G32(ZOOM) * z) / 5;   // golf's own lift: FUN_0040f5c0
  int lift = (t + ((t >> 31) & 0xf)) >> 4;
  r->slot = (int16_t)slot;
  r->kind = (uint8_t)kind;
  r->wx = G32(BALL_X + slot * 0x100);
  r->wy = G32(BALL_Y + slot * 0x100);
  r->wz = z;
  r->gx = (float)sx;
  r->gy = (float)sy;
  r->lift = (float)lift;
  r->occ_ball = r->occ_shadow = 1.0f;
  g.stamp[slot] = g.frame;
  return 1;
}

int hb_match(int kind, int sx, int sort_y) {
  for (int s = 0; s < HB_MAX; s++) {
    if (g.stamp[s] != g.frame) continue;
    const HdBallRec *r = &g.rec[s];
    int gy = (int)r->gy;
    if ((int)r->gx == sx && (kind == 2 ? gy == sort_y + 1 : gy == sort_y)) return s;
  }
  return -1;
}

void hb_snap_begin(int kind, int rec, int x0, int y0, int w, int h) {
  g.open = 0;
  if (g.npend >= MAX_PEND || w > 16 || h > 16) return;
  int pitch, bw, bh;
  uint16_t *bits = hd_ui_back_bits(&pitch, &bw, &bh);
  if (!bits) return;
  if (x0 < 0) w += x0, x0 = 0;
  if (y0 < 0) h += y0, y0 = 0;
  if (x0 + w > bw) w = bw - x0;
  if (y0 + h > bh) h = bh - y0;
  if (w <= 0 || h <= 0) return;
  Pend *p = &g.pend[g.npend];
  p->kind = kind;
  p->rec = rec;
  p->x0 = x0, p->y0 = y0, p->w = w, p->h = h;
  p->n_changed = 0;
  for (int y = 0; y < h; y++) memcpy(p->before + y * 16, bits + (size_t)(y0 + y) * pitch + x0, (size_t)w * 2);
  g.open = 1;
}

void hb_snap_end(void) {
  if (!g.open) return;
  g.open = 0;
  int pitch, bw, bh;
  uint16_t *bits = hd_ui_back_bits(&pitch, &bw, &bh);
  if (!bits) return;
  Pend *p = &g.pend[g.npend];
  int n = 0;
  for (int y = 0; y < p->h; y++) {
    memcpy(p->after + y * 16, bits + (size_t)(p->y0 + y) * pitch + p->x0, (size_t)p->w * 2);
    for (int x = 0; x < p->w; x++) n += p->after[y * 16 + x] != p->before[y * 16 + x];
  }
  p->n_changed = n;
  static int dbg = -1, logged;
  if (dbg < 0) dbg = getenv("SIMGOLF_BALL_DEBUG") != NULL;
  if (dbg && logged < 60) {
    int x0 = 99, y0 = 99, x1 = -1, y1 = -1;
    for (int y = 0; y < p->h; y++)
      for (int x = 0; x < p->w; x++)
        if (p->after[y * 16 + x] != p->before[y * 16 + x]) {
          if (x < x0) x0 = x;
          if (x > x1) x1 = x;
          if (y < y0) y0 = y;
          if (y > y1) y1 = y;
        }
    logged++;
    fprintf(stderr, "ball: snap kind %d rec %d rect %d,%d %dx%d changed %d bbox %d..%d,%d..%d\n", p->kind, p->rec, p->x0, p->y0, p->w, p->h, n,
            x0, x1, y0, y1);
  }
  if (n) g.npend++;
}

void hdball_resolve(void) {
  if (!g.npend) return;
  int pitch, bw, bh;
  uint16_t *bits = hd_ui_back_bits(&pitch, &bw, &bh);
  if (!bits) {
    g.npend = 0;
    return;
  }
  // newest first: where two draws overlap, undoing them in reverse order
  // leaves what was there before either
  for (int i = g.npend - 1; i >= 0; i--) {
    Pend *p = &g.pend[i];
    int total = 0, kept = 0;
    for (int y = 0; y < p->h; y++)
      for (int x = 0; x < p->w; x++) {
        uint16_t b = p->before[y * 16 + x], a = p->after[y * 16 + x];
        if (a == b) continue;
        total++;
        uint16_t *px = bits + (size_t)(p->y0 + y) * pitch + p->x0 + x;
        static int dbg = -1, logged;
        if (dbg < 0) dbg = getenv("SIMGOLF_BALL_DEBUG") != NULL;
        if (dbg && *px != a && logged < 60) {
          logged++;
          fprintf(stderr, "ball: covered px kind %d rec %d at %d,%d: before %04x after %04x now %04x\n", p->kind, p->rec, p->x0 + x, p->y0 + y,
                  b, a, *px);
        }
        if (*px == a) {
          kept++;
          *px = b;
        }
      }
    if (p->rec >= 0 && p->rec < HB_MAX && g.stamp[p->rec] == g.frame && total) {
      float v = (float)kept / (float)total;
      if (p->kind == 2) g.rec[p->rec].occ_shadow = v;
      else g.rec[p->rec].occ_ball = v;
    }
  }
  g.npend = 0;
}

void hdball_publish(HdBallFrame *out) {
  out->on = g.decided ? g.on : hdball_on();
  out->zoom2 = G32(ZOOM2) != 0;
  out->frame = g.frame;
  out->t_ms = host_now_ms();
  int n = 0;
  if (out->on)
    for (int s = 0; s < HB_MAX; s++)
      if (g.stamp[s] == g.frame) out->b[n++] = g.rec[s];
  out->n = n;
  g.frame++;
  g.decided = 0;
  g.npend = 0;
  g.open = 0;
}
