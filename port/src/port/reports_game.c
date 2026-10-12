// Frosted Mint reports, game-thread side, native build (simgolf mod, task-28). Overview in
// port/reports_frosted.h. This file does not include golf_protos.h (its types clash with the C library
// headers).
//
// Hooks (translator patches, port/tools/translate/golf.reports.json):
//   rp_tick      once per main-loop frame: keeps the native layer up, opens the report a tab asked for.
//   rp_begin     the main loop is about to run report N's modal loop: keep the picture.
//   rp_t650 ...  the report's text calls: record the text, then draw it as before.
//   rp_frame     once per iteration of the report's loop (and before its first present): publish, put the
//                picture back (golf's board never shows), run the native actions.
//   rp_done      the report returned.
#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/hd_native.h"
#include "port/reports_frosted.h"

#define G32(a) (*(volatile int32_t *)(uintptr_t)(a))
#define SCREEN_STATE 0x569498
#define PAGE_WIDGET 0x519cd4   // the page the reports draw their text into
#define TITLE_FONT 0x821020    // the title font of the report boards

void FUN_00476650(void *w, int font, char *s, int x, int y, int len);
void FUN_004766a0(void *w, int font, char *s, int x, int y, int wd, char *len);
void FUN_00476700(void *w, int font, char *s, int x, int y, char *len);
void FUN_00476750(void *w, int font, char *s, int x, int y, char *len);
void FUN_00476310(void *w, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
int FUN_00404b70(char *s, int x, int y, uint32_t col);
int FUN_00404bc0(char *s, int x, int y, uint32_t col);
int FUN_004049d0(char *s, int x, int y, uint32_t col);
int FUN_00404ad0(char *s, int x, int y, uint32_t col);
void FUN_00480c80(void *page);   // flush the page's changes to the screen
int FUN_00455a30(void);          // best scores (modal)

static int g_on;          // this report: the native panel replaces the original
static int g_kind;
static int g_log = -1;
static int g_closing;     // a close was asked: the key is on its way
static int g_pending;     // a tab asked for this report once the current one has closed
static uint32_t g_serial;
static RpSnap g_snap;
static uint16_t g_col;    // colour set by the last FUN_00476310 on the page widget
static int g_dirty;       // the report drew since the last frame: its picture must be taken off the screen

// ------------------------------------------------------------------ the picture from before the report

uint16_t *gdi_screen_dib(int i, int *w, int *h, int *pitch, int *is565);
enum { PAGES = 8 };
static uint16_t *g_keep[PAGES];
static int g_keep_w[PAGES], g_keep_h[PAGES];

static void keep_free(void) {
  for (int i = 0; i < PAGES; i++) {
    free(g_keep[i]);
    g_keep[i] = NULL;
  }
}

static void keep_save(void) {
  keep_free();
  for (int i = 0; i < PAGES; i++) {
    int w, h, pitch, is565;
    uint16_t *bb = gdi_screen_dib(i, &w, &h, &pitch, &is565);
    if (!bb) continue;
    g_keep[i] = malloc((size_t)w * h * 2);
    if (!g_keep[i]) continue;
    for (int y = 0; y < h; y++) memcpy(g_keep[i] + (size_t)y * w, bb + (ptrdiff_t)y * pitch, (size_t)w * 2);
    g_keep_w[i] = w, g_keep_h[i] = h;
  }
}

static void keep_restore(void) {
  for (int i = 0; i < PAGES; i++) {
    if (!g_keep[i]) continue;
    int w, h, pitch, is565;
    uint16_t *bb = gdi_screen_dib(i, &w, &h, &pitch, &is565);
    if (!bb || w != g_keep_w[i] || h != g_keep_h[i]) continue;
    for (int y = 0; y < h; y++) memcpy(bb + (ptrdiff_t)y * pitch, g_keep[i] + (size_t)y * w, (size_t)w * 2);
  }
}

// ------------------------------------------------------------------ injected input

static void push_key(SDL_Keycode k, int down) {
  SDL_Event e = {0};
  e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.key.keysym.sym = k;
  e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
  SDL_PushEvent(&e);
}

static int g_inj, g_inj_x, g_inj_y, g_inj_stage, g_inj_wait;

static void sdl_motion(int x, int y) {
  SDL_Event e = {0};
  e.type = SDL_MOUSEMOTION;
  e.motion.x = x;
  e.motion.y = y;
  SDL_PushEvent(&e);
}

static void sdl_button(int x, int y, int down) {
  SDL_Event e = {0};
  e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.button.clicks = 1;
  e.button.x = x;
  e.button.y = y;
  SDL_PushEvent(&e);
}

// One step per frame: move, press, release.
static void inj_step(void) {
  if (!g_inj) return;
  if (g_inj_wait > 0) {
    g_inj_wait--;
    return;
  }
  switch (g_inj_stage) {
    case 0: sdl_motion(g_inj_x, g_inj_y); g_inj_wait = 1; break;
    case 1: sdl_button(g_inj_x, g_inj_y, 1); g_inj_wait = 2; break;
    case 2: sdl_button(g_inj_x, g_inj_y, 0); g_inj_wait = 1; break;
    default: g_inj = 0; return;
  }
  g_inj_stage++;
}

// ------------------------------------------------------------------ hooks

static int is_overlay(int k) {
  return k == RP_MONTH || k == RP_COURSE || k == RP_COMMENTS || k == RP_HISTO || k == RP_KEYS || k == RP_FINANCE || k == RP_SGA ||
         k == RP_ROSTER;
}

void rp_done(void);

void rp_tick(void) {
  int on = rpf_enabled();   // keeps the layer registered and drawing before a report opens
  if (g_on && g_kind == RP_MONTH && G32(SCREEN_STATE) != RP_MONTH) rp_done();   // the per-frame report went away
  if (!g_pending) return;
  int k = g_pending;
  g_pending = 0;
  if (!on) return;
  if (g_log) fprintf(stderr, "reports: open %d from a tab\n", k);
  if (is_overlay(k)) G32(SCREEN_STATE) = k;
  else if (k == RP_BEST) {
    void rp_begin(int kind);
    rp_begin(RP_BEST);
    FUN_00455a30();
    rp_done();
  }
}

void rp_begin(int kind) {
  if (g_log < 0) g_log = getenv("SIMGOLF_REPORTS_LOG") != NULL;
  g_on = rpf_enabled();
  g_kind = kind;
  g_closing = 0;
  g_inj = 0;
  g_col = 0;
  g_dirty = 1;
  memset(&g_snap, 0, offsetof(RpSnap, it));
  g_snap.magic = RP_MAGIC;
  g_snap.kind = kind;
  if (g_log) fprintf(stderr, "reports: open %d native %d\n", kind, g_on), fflush(stderr);
  if (g_on) {
    hd_ui_show_golf_ui(0);
    keep_save();
  }
}

static void copy_str(char *dst, int cap, const char *src, int len) {
  int n = 0;
  if (len < 0 || len > cap - 1) len = cap - 1;
  while (n < len && src && src[n]) {
    dst[n] = src[n];
    n++;
  }
  dst[n] = 0;
}

// One thing the report drew. Drawn again at the same place, it replaces the old one (the loops redraw rows
// when they scroll or hover).
static RpItem *slot(int type, int x, int y) {
  RpSnap *p = &g_snap;
  g_dirty = 1;
  for (int i = 0; i < p->n; i++)
    if (p->it[i].x == x && p->it[i].y == y && p->it[i].type == type) return &p->it[i];
  if (p->n >= RP_ITEMS) return NULL;
  RpItem *it = &p->it[p->n++];
  memset(it, 0, sizeof *it);
  it->type = (uint8_t)type;
  it->x = (int16_t)x;
  it->y = (int16_t)y;
  return it;
}

static RpItem *record(const char *s, int len, int x, int y, int align, int big) {
  if (!g_on || !s) return NULL;
  RpSnap *p = &g_snap;
  if (big && !p->title[0]) {
    copy_str(p->title, sizeof p->title, s, len);
    return NULL;
  }
  RpItem *it = slot(RP_IT_TEXT, x, y);
  if (!it) return NULL;
  it->align = (uint8_t)align;
  it->big = (uint8_t)big;
  it->col = g_col;
  copy_str(it->s, sizeof it->s, s, len);
  return it;
}

void FUN_00478b80(void *w, int x0, int y0, int x1, int y1, uint32_t col);
void FUN_00480b00(void *page, int x, int y, int w, int h, uint32_t col);
int FUN_00473e60(void *sprite, int target, int x, int y, int clip);

// golf's line (the histograph's curves)
void rp_line(void *w, int x0, int y0, int x1, int y1, uint32_t col) {
  if (g_on) {
    RpItem *it = slot(RP_IT_LINE, x0, y0);
    if (it) it->w = (int16_t)x1, it->h = (int16_t)y1, it->col = (uint16_t)col;
  }
  FUN_00478b80(w, x0, y0, x1, y1, col);
}

// golf's filled box (the histograph's event dots, the roster's scroll thumb)
void rp_rect(void *page, int x, int y, int w, int h, uint32_t col) {
  if (g_on) {
    RpItem *it = slot(RP_IT_RECT, x, y);
    if (it) it->w = (int16_t)w, it->h = (int16_t)h, it->col = (uint16_t)col;
  }
  FUN_00480b00(page, x, y, w, h, col);
}

// The page the report draws on: the one whose pixel at (x, y) now differs from the kept picture.
static int drawn_page(int x, int y) {
  for (int i = 0; i < PAGES; i++) {
    if (!g_keep[i]) continue;
    int w, h, pitch, is565;
    uint16_t *bb = gdi_screen_dib(i, &w, &h, &pitch, &is565);
    if (!bb || w != g_keep_w[i] || h != g_keep_h[i] || x < 0 || y < 0 || x >= w || y >= h) continue;
    if (bb[(ptrdiff_t)y * pitch + x] != g_keep[i][(size_t)y * w + x]) return i;
  }
  return -1;
}

// golf's sprites in a report that carry meaning (the roster's status balls, photo and heart marks): kept
// as golf's own art. Each sprite is read once, with alpha, and published to the render thread.
static struct {
  void *spr;
  int page, done;
  int plain;        // the sprite is one flat colour (a highlight box): drawn as a tinted chip
  uint16_t col;
} g_art[RP_ART];
static int g_nart;

static int sprite_size(void *spr, int *w, int *h, int *ox, int *oy) {
  *w = G32((uintptr_t)spr + 0x10), *h = G32((uintptr_t)spr + 0x14);   // the sprite's size; +0x20/+0x24 its offset
  *ox = G32((uintptr_t)spr + 0x20), *oy = G32((uintptr_t)spr + 0x24);
  return *w > 0 && *h > 0 && *w <= 128 && *h <= 128;
}

int rp_chip(void *sprite, int target, int x, int y, int clip) {
  int r = FUN_00473e60(sprite, target, x, y, clip);
  int w, h, ox, oy;
  uintptr_t sp = (uintptr_t)sprite;
  if (sp == 0x820f40 || sp == 0x820f6c || sp == 0x820fc4) return r;   // the board's close button (the panel has its own)
  if (!g_on || !sprite_size(sprite, &w, &h, &ox, &oy)) return r;
  int k = 0;
  while (k < g_nart && g_art[k].spr != sprite) k++;
  if (k == g_nart) {
    if (g_nart == RP_ART) return r;
    g_art[k].spr = sprite;
    g_art[k].page = drawn_page(x + ox + w / 2, y + oy + h / 2);
    g_art[k].done = 0;
    g_nart++;
  }
  RpItem *it = slot(RP_IT_CHIP, x + ox, y + oy);
  if (it) {
    it->w = (int16_t)w, it->h = (int16_t)h, it->art = (uint8_t)(k + 1);
    it->big = (uint8_t)g_art[k].plain, it->col = g_art[k].col;
  }
  return r;
}

// Draws each new sprite into a corner of its page over black and over white (the page is put back right
// after) and turns the two into ARGB: a = 1 - (white - black), colour = black / a.
static void capture_art(void) {
  for (int k = 0; k < g_nart; k++) {
    if (g_art[k].done) continue;
    g_art[k].done = 1;
    int sw, sh, ox, oy, w, h, pitch, is565;
    if (!sprite_size(g_art[k].spr, &sw, &sh, &ox, &oy) || g_art[k].page < 0) continue;
    uint16_t *bb = gdi_screen_dib(g_art[k].page, &w, &h, &pitch, &is565);
    if (!bb || sw + 8 > w || sh + 8 > h) continue;
    uint16_t *shot = malloc((size_t)sw * sh * 2 * 2);
    uint32_t *px = malloc((size_t)sw * sh * 4);
    if (!shot || !px) {
      free(shot), free(px);
      continue;
    }
    uint16_t white = is565 ? 0xffff : 0x7fff;
    for (int pass = 0; pass < 2; pass++) {
      for (int y = 0; y < sh + 8; y++)
        for (int x = 0; x < sw + 8; x++) bb[(ptrdiff_t)y * pitch + x] = pass ? white : 0;
      FUN_00473e60(g_art[k].spr, PAGE_WIDGET, 4 - ox, 4 - oy, 0);
      for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++) shot[(size_t)pass * sw * sh + y * sw + x] = bb[(ptrdiff_t)(y + 4) * pitch + x + 4];
    }
    for (int i = 0; i < sw * sh; i++) {
      uint16_t b = shot[i], wv = shot[sw * sh + i];
      int br, bg, bl, wr, wg, wb;
      if (is565) br = (b >> 11) & 31, bg = (b >> 6) & 31, bl = b & 31, wr = (wv >> 11) & 31, wg = (wv >> 6) & 31, wb = wv & 31;
      else br = (b >> 10) & 31, bg = (b >> 5) & 31, bl = b & 31, wr = (wv >> 10) & 31, wg = (wv >> 5) & 31, wb = wv & 31;
      int a = 255 - ((wr - br) + (wg - bg) + (wb - bl)) * 255 / (3 * 31);
      a = a < 0 ? 0 : a > 255 ? 255 : a;
      uint32_t v = 0;
      if (a > 0) {
        int rr = br * 255 / 31 * 255 / a, gg = bg * 255 / 31 * 255 / a, b2 = bl * 255 / 31 * 255 / a;
        rr = rr > 255 ? 255 : rr, gg = gg > 255 ? 255 : gg, b2 = b2 > 255 ? 255 : b2;
        v = (uint32_t)a << 24 | (uint32_t)rr << 16 | (uint32_t)gg << 8 | (uint32_t)b2;
      }
      px[i] = v;
    }
    // one flat opaque colour: a highlight box, not a picture
    int same = 0;
    uint16_t c0 = shot[(sh / 2) * sw + sw / 2];
    for (int i = 0; i < sw * sh; i++) same += shot[i] == c0 && shot[sw * sh + i] == c0;
    if (sw >= 16 && same * 10 >= sw * sh * 8) {
      g_art[k].plain = 1, g_art[k].col = c0;
      for (int i = 0; i < g_snap.n; i++)
        if (g_snap.it[i].type == RP_IT_CHIP && g_snap.it[i].art == k + 1) g_snap.it[i].big = 1, g_snap.it[i].col = c0;
    }
    rpf_publish_art(k, px, sw, sh);
    if (g_log) fprintf(stderr, "reports: art %d %dx%d\n", k, sw, sh);
    free(shot), free(px);
  }
}

void rp_quad(void *w, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  if ((uintptr_t)w == PAGE_WIDGET) g_col = (uint16_t)(a & 0xffff);
  FUN_00476310(w, a, b, c, d);
}

void rp_t650(void *w, int font, char *s, int x, int y, int len) {
  if ((uintptr_t)w == PAGE_WIDGET) record(s, len, x, y, RP_AL_LEFT, font == TITLE_FONT);
  FUN_00476650(w, font, s, x, y, len);
}

void rp_t6a0(void *w, int font, char *s, int x, int y, int wd, char *len) {
  if ((uintptr_t)w == PAGE_WIDGET) record(s, (int)(intptr_t)len, x + wd / 2, y, RP_AL_CENTRE, font == TITLE_FONT);
  FUN_004766a0(w, font, s, x, y, wd, len);
}

void rp_t700(void *w, int font, char *s, int x, int y, char *len) {
  if ((uintptr_t)w == PAGE_WIDGET) record(s, (int)(intptr_t)len, x, y, RP_AL_LEFT, font == TITLE_FONT);
  FUN_00476700(w, font, s, x, y, len);
}

void rp_t750(void *w, int font, char *s, int x, int y, char *len) {
  if ((uintptr_t)w == PAGE_WIDGET) record(s, (int)(intptr_t)len, x, y, RP_AL_RIGHT, font == TITLE_FONT);
  FUN_00476750(w, font, s, x, y, len);
}

// golf's plain text calls (centred: FUN_00404b70 and FUN_00404bc0; left: FUN_004049d0 and FUN_00404ad0)
int rp_tc(int fn, char *s, int x, int y, uint32_t col) {
  RpItem *it = record(s, -1, x, y, fn < 2 ? RP_AL_CENTRE : RP_AL_LEFT, 0);
  if (it) it->col = (uint16_t)col;
  switch (fn) {
    case 0: return FUN_00404b70(s, x, y, col);
    case 1: return FUN_00404bc0(s, x, y, col);
    case 2: return FUN_004049d0(s, x, y, col);
    default: return FUN_00404ad0(s, x, y, col);
  }
}

void rp_frame(void) {
  if (!g_on) return;
  g_snap.serial = ++g_serial;
  rpf_publish(&g_snap);
  RpAction a;
  while (rpf_take_action(&a)) {
    if (a.kind == RP_A_CLOSE || a.kind == RP_A_TAB) {
      if (a.kind == RP_A_TAB && a.arg != g_kind) g_pending = a.arg;
      if (!g_closing) {
        g_closing = 1;
        push_key(SDLK_ESCAPE, 1);
        push_key(SDLK_ESCAPE, 0);
      }
    } else if (a.kind == RP_A_CLICK && !g_inj) {
      g_inj = 1;
      g_inj_x = a.arg & 0xffff;
      g_inj_y = (a.arg >> 16) & 0xffff;
      g_inj_stage = g_inj_wait = 0;
      if (g_log) fprintf(stderr, "reports: click at the original's %d,%d\n", g_inj_x, g_inj_y);
    }
  }
  inj_step();
  capture_art();
  keep_restore();
  if (g_dirty) FUN_00480c80((void *)0x519a60);   // only when golf drew: a waiting report presents no frames
  g_dirty = 0;
}

// golf's drawString (the tournament results' right column)
int FUN_00478140(void *w, char *s, int x, int y, char *len);
int rp_ds(void *w, char *s, int x, int y, char *len) {
  if ((uintptr_t)w == PAGE_WIDGET) record(s, (int)(intptr_t)len, x, y, RP_AL_RIGHT, 0);
  return FUN_00478140(w, s, x, y, len);
}

// A report golf draws every frame of its main loop instead of in a loop of its own (the end of month
// ranking, screen state 8): phase 0 before golf draws it, phase 1 after. Not wired yet: that box and the
// tournament leaderboard slide in beside the running game and keep the original look (task-28 notes). The panel stays up while the
// frames keep coming and goes when they stop (the main loop clears the state on any key).
void rp_ov(int kind, int phase) {
  if (phase == 0) {
    if (!(g_on && g_kind == kind)) {
      if (rpf_enabled()) rp_begin(kind);   // with ui.reports=original: nothing, every frame
    } else {
      keep_save();
      g_snap.n = 0;
      g_snap.title[0] = 0;
    }
    return;
  }
  if (!g_on || g_kind != kind) return;
  // the end of month box slides in from the side: its frame (the first box) is at 0x176 when it stands
  for (int i = 0; i < g_snap.n; i++)
    if (g_snap.it[i].type == RP_IT_RECT) {
      int dx = g_snap.it[i].x - 0x176;
      for (int j = 0; dx && j < g_snap.n; j++) {
        g_snap.it[j].x = (int16_t)(g_snap.it[j].x - dx);
        if (g_snap.it[j].type == RP_IT_LINE) g_snap.it[j].w = (int16_t)(g_snap.it[j].w - dx);
      }
      break;
    }
  g_snap.serial = ++g_serial;
  rpf_publish(&g_snap);
  RpAction a;
  while (rpf_take_action(&a))
    if (a.kind == RP_A_CLOSE || a.kind == RP_A_TAB) {
      if (a.kind == RP_A_TAB && a.arg != g_kind) g_pending = a.arg;
      push_key(SDLK_ESCAPE, 1);
      push_key(SDLK_ESCAPE, 0);
    }
  capture_art();
  keep_restore();
}

void rp_done(void) {
  if (g_on) {
    if (g_log) fprintf(stderr, "reports: closed %d (%d texts)\n", g_kind, g_snap.n);
    if (g_log > 0 && getenv("SIMGOLF_REPORTS_DUMP"))
      for (int i = 0; i < g_snap.n; i++)
        fprintf(stderr, "reports: text %d,%d a%d c%04x '%s'\n", g_snap.it[i].x, g_snap.it[i].y, g_snap.it[i].align,
                g_snap.it[i].col, g_snap.it[i].s);
    if (g_log) fflush(stderr);
    rpf_end();
    keep_restore();
    hd_ui_show_golf_ui(1);
    keep_free();
  }
  g_on = 0;
  g_kind = 0;
  g_inj = 0;
}
