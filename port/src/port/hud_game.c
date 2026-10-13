// Frosted Mint HUD, game-thread side, native build (simgolf mod, task-30).
// The overview is at the top of golf/hud_hooks.c (which holds the news strip
// override and the browser build's no-op versions of these hooks). This file
// does not include golf_protos.h (its types clash with the C library headers);
// the few game functions it calls are declared below with the same ABI.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/dialogs_frosted.h"
#include "port/hud_frosted.h"
#include "port/host.h"

#define G32(a) (*(volatile int32_t *)(uintptr_t)(a))
#define G16(a) (*(volatile int16_t *)(uintptr_t)(a))
#define G8(a) (*(volatile int8_t *)(uintptr_t)(a))

#define FLAGS 0x59e7b8
#define DOCK_MODE 0x567afc
#define SCREEN_STATE 0x569498
#define TEXT_BUF 0x51a068
#define NEWS_TEXT 0x5a6d40
#define MOUSE_X 0x822d60
#define MOUSE_Y 0x822d64

// translated game functions (build/gen/golf/golf_protos.h)
uint32_t FUN_004657e0(int x, int y);                                                       // left click entry
int FUN_00473cb0(void *sprite, int target, int x, int y, int sx, int sy, int div, int clip);  // draw sprite
uint32_t FUN_00473f60(void *sprite, int mask, int target, uint32_t x, uint32_t y, int clip);  // draw sprite + alpha
int FUN_00473e60(void *sprite, int target, int x, int y, int clip);                         // draw sprite
int FUN_0040daa0(int what);                                                                // course name -> text_buf
uint32_t FUN_0040d7b0(int tick);                                                           // date -> text_buf
uint32_t FUN_00407280(int hole);                                                           // hole name -> appended to text_buf
int FUN_004065c0(void);                                                                    // the pro's first skill setup (10 points, 3 per accomplishment)
int FUN_0045f0f0(char *title, uint32_t mask, int points, int slot, int xoff);              // the skill dialog
uint32_t FUN_004385d0(int a, int b);                                                       // the golfer profile page
int FUN_0040bfa0(int x, int y);                                                            // terrain at a world point
const char *mods_get(const char *key);


int pv_building_of_text(const char *text);   // paths_game.c (task-53)
void pv_show_building(int idx);
int gw_native_card(void);   // golfers_game.c: the Frosted Mint golfer card replaces the original

// hd_art.c (task-81): the art pack's hi-res copies of what the old HUD drew go with its pixels
void hd_art_hide_begin(void);
void hd_art_hide_rects(const int *xywh, int nrects);

// gdi.c: golf's screen-sized 16-bit page buffers (it may flip between two)
uint16_t *gdi_screen_dib(int i, int *w, int *h, int *pitch, int *is565);
extern int win_screen_w, win_screen_h;

#define PAGES 8
static int g_on;              // this frame: the new HUD replaces the old one
static uint16_t *g_save[PAGES];
static size_t g_save_cap[PAGES];
static int g_target = -1;     // the page the old HUD drew into this frame (found in hud_restore)
typedef struct { int x, y, w, h; } R;
static R g_rects[3];
static int g_nrects;

// injection
static int g_inj, g_inj_x, g_inj_y, g_inj_frames;
static HudAction g_goal;
static int g_goal_on, g_goal_steps;

static HudSnap g_snap;
static uint32_t g_serial, g_art_serial;
static int g_art_theme = -1, g_art_frames;

int hud_on(void) { return g_on; }
int hud_mx(int x) { return g_on ? -10000 : x; }
int hud_my(int y) { return g_on ? -10000 : y; }
int hud_ylim(int y) { return g_on ? 100000 : y; }
int hud_injecting(void) { return g_inj; }

// Scripted clicks (harness, bridge: SDL events without a window, in frame
// pixels) still reach the old wheel and trays, so scripts and tools written
// for the original layout keep working with the new HUD on.
static int g_script_x = -1, g_script_y = -1;
void hud_note_script_click(int x, int y) {
  g_script_x = x;
  g_script_y = y;
}
static int script_click(void) { return G32(MOUSE_X) == g_script_x && G32(MOUSE_Y) == g_script_y; }

int hud_strip_click(void) { return !g_on || (!g_inj && script_click()); }

int hud_old_click(void) {
  if (!g_on) return 1;
  if (!g_inj) return script_click();
  // the click being handled is ours: make sure it carries our point
  G32(MOUSE_X) = g_inj_x;
  G32(MOUSE_Y) = g_inj_y;
  g_inj = 0;
  hudf_set_injecting(0);
  return 1;
}

static void rects(void) {
  int W = win_screen_w, H = win_screen_h;
  g_nrects = 0;
  g_rects[g_nrects++] = (R){0, 0, 240, 84};                      // banner, stars, hearts
  g_rects[g_nrects++] = (R){W - 215, 0, 215, 142};               // money, fun, skill plates
  g_rects[g_nrects++] = (R){0, H - 220, W, 220};                 // wheel, trays, chip strip, tray tooltips
                                                                   // (bottom-anchored on a tall canvas, task-3 B4)
  for (int i = 0; i < g_nrects; i++) {
    R *r = &g_rects[i];
    if (r->x < 0) r->w += r->x, r->x = 0;
    if (r->y < 0) r->h += r->y, r->y = 0;
    if (r->x + r->w > W) r->w = W - r->x;
    if (r->y + r->h > H) r->h = H - r->y;
    if (r->w < 0) r->w = 0;
    if (r->h < 0) r->h = 0;
  }
}

// mode 1: keep page i's pixels under the old HUD; 0: put them back;
// 2: compare (returns 1 when the old HUD drew into page i since the save).
static int copy_rects(int page, int mode) {
  int w, h, pitch, is565;
  uint16_t *bb = gdi_screen_dib(page, &w, &h, &pitch, &is565);
  if (!bb) return 0;
  size_t need = 0;
  for (int i = 0; i < g_nrects; i++) need += (size_t)g_rects[i].w * g_rects[i].h;
  if (need > g_save_cap[page]) {
    g_save[page] = realloc(g_save[page], need * 2);
    g_save_cap[page] = need;
  }
  uint16_t *p = g_save[page];
  for (int i = 0; i < g_nrects; i++) {
    R r = g_rects[i];
    if (r.x + r.w > w) r.w = w - r.x;
    if (r.y + r.h > h) r.h = h - r.y;
    for (int y = 0; y < r.h; y++) {
      uint16_t *row = bb + (ptrdiff_t)(r.y + y) * pitch + r.x;
      if (mode == 1) memcpy(p, row, (size_t)r.w * 2);
      else if (mode == 0) memcpy(row, p, (size_t)r.w * 2);
      else if (memcmp(row, p, (size_t)r.w * 2)) return 1;
      p += g_rects[i].w;
    }
  }
  return 0;
}

void hud_save(void) {
  dlgf_enabled();   // task-27: registers the dialogs layer early (a dialog needs it up before it opens)
  int w, h, pitch, is565;
  g_on = hudf_enabled() && gdi_screen_dib(0, &w, &h, &pitch, &is565) != NULL;
  if (!g_on) return;
  rects();
  for (int i = 0; i < PAGES; i++) copy_rects(i, 1);
  hd_art_hide_begin();
}

static void hide_art(void) { hd_art_hide_rects(&g_rects[0].x, g_nrects); }

// ------------------------------------------------------------------ art

// Draws a game sprite into a scratch box of the back buffer (inside the
// area hud_restore puts back) over black and over white, and turns the two
// into ARGB with alpha: a = 1 - (white - black), colour = black / a.
// kind: 0 FUN_00473cb0 (plain), 1 FUN_00473f60 (with its alpha sprite mask), 2 FUN_00473e60
static uint32_t *snap_sprite(void *sprite, int kind, void *mask, int *ow, int *oh) {
  int w, h, pitch, is565;
  uint16_t *bb = g_target >= 0 ? gdi_screen_dib(g_target, &w, &h, &pitch, &is565) : NULL;
  const int BX = 4, BY = win_screen_h - 210, BW = 196, BH = 200;   // inside the bottom rectangle hud_restore puts back
  if (!bb || BX + BW > w || BY + BH > h) return NULL;
  static uint16_t shot[2][196 * 200];
  for (int pass = 0; pass < 2; pass++) {
    uint16_t fillc = pass ? (is565 ? 0xffff : 0x7fff) : 0;
    for (int y = 0; y < BH; y++)
      for (int x = 0; x < BW; x++) bb[(ptrdiff_t)(BY + y) * pitch + BX + x] = fillc;
    int ox = G32((uintptr_t)sprite + 0x20), oy = G32((uintptr_t)sprite + 0x24);
    int dx = BX + 8 - (ox > 0 ? 0 : ox), dy = BY + 8 - (oy > 0 ? 0 : oy);
    if (kind == 1) FUN_00473f60(sprite, (int)(uintptr_t)mask, G32(0x4c1570), dx, dy, 0);
    else if (kind == 2) FUN_00473e60(sprite, G32(0x4c1570), dx, dy, 0);
    else FUN_00473cb0(sprite, G32(0x4c1570), dx, dy, 1, 1, 1, 0);
    for (int y = 0; y < BH; y++)
      for (int x = 0; x < BW; x++) shot[pass][y * BW + x] = bb[(ptrdiff_t)(BY + y) * pitch + BX + x];
  }
  int x0 = BW, y0 = BH, x1 = -1, y1 = -1;
  for (int y = 0; y < BH; y++)
    for (int x = 0; x < BW; x++)
      if (shot[0][y * BW + x] != 0 || shot[1][y * BW + x] != (is565 ? 0xffff : 0x7fff)) {
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
  if (x1 < x0) return NULL;
  int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
  uint32_t *out = malloc((size_t)cw * ch * 4);
  for (int y = 0; y < ch; y++)
    for (int x = 0; x < cw; x++) {
      uint16_t b = shot[0][(y0 + y) * BW + x0 + x], wv = shot[1][(y0 + y) * BW + x0 + x];
      int br, bg, bb2, wr, wg, wb;
      if (is565) {
        br = (b >> 11) & 31, bg = (b >> 6) & 31, bb2 = b & 31;
        wr = (wv >> 11) & 31, wg = (wv >> 6) & 31, wb = wv & 31;
      } else {
        br = (b >> 10) & 31, bg = (b >> 5) & 31, bb2 = b & 31;
        wr = (wv >> 10) & 31, wg = (wv >> 5) & 31, wb = wv & 31;
      }
      int d = ((wr - br) + (wg - bg) + (wb - bb2)) * 255 / (3 * 31);
      int a = 255 - d;
      if (a < 0) a = 0;
      if (a > 255) a = 255;
      uint32_t px = 0;
      if (a > 0) {
        int r = br * 255 / 31 * 255 / a, g = bg * 255 / 31 * 255 / a, bl = bb2 * 255 / 31 * 255 / a;
        r = r > 255 ? 255 : r, g = g > 255 ? 255 : g, bl = bl > 255 ? 255 : bl;
        px = (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
      }
      out[y * cw + x] = px;
    }
  *ow = cw;
  *oh = ch;
  return out;
}

static void publish_sprite(int slot, void *sprite, int kind, void *mask) {
  int w = 0, h = 0;
  uint32_t *px = snap_sprite(sprite, kind, mask, &w, &h);
  hudf_publish_art(slot, px, w, h);
  free(px);
}

static void snap_art(void) {
  // terrain slots: sprite 0x570cb8 + (state + slot*4) * 0x2c, state 0 = normal
  for (int i = 0; i < 16; i++) publish_sprite(HUD_ART_TERRAIN + i, (void *)(uintptr_t)(0x570cb8 + i * 4 * 0x2c), 0, NULL);
  // the course emblem, as the banner draws it
  int prop = G32(0x59bf90);
  int kind = (int)(signed char)G8(0x571ff4 + prop * 0x2e);
  int idx = (int)(signed char)G8(0x4c1ea8 + kind * 0x82);
  if (idx >= 0 && idx < 64)
    publish_sprite(HUD_ART_EMBLEM, (void *)(uintptr_t)(0x541710 + idx * 0x2c), 1, (void *)(uintptr_t)(0x5791f8 + idx * 0x2c));
  // building lots (FUN_00434350): 0x53df58 + (enabled + (upgraded + lot*2)*2) * 0x2c
  for (int i = 0; i < 9; i++) {
    int up = G32(0x5a8c50 + i * 4) != 0;
    publish_sprite(HUD_ART_BUILDING + i, (void *)(uintptr_t)(0x53df58 + (1 + (up + i * 2) * 2) * 0x2c), 0, NULL);
  }
  // amenities (FUN_00434cf0): the lit look of each control, 0x59ba68 + slot * 0x84, in ui_hud.c's order
  static const int amen_slot[8] = {1, 3, 5, 0, 4, 2, 6, 9};
  for (int i = 0; i < 8; i++)
    publish_sprite(HUD_ART_AMENITY + i, (void *)(uintptr_t)(0x59ba68 + amen_slot[i] * 0x84), 2, NULL);
  // elevation tools (FUN_00433e50): the selected look, 0x571d6c + tool * 0x84
  for (int i = 0; i < 4; i++) publish_sprite(HUD_ART_ELEVATION + i, (void *)(uintptr_t)(0x571d6c + i * 0x84), 2, NULL);
  g_art_serial++;
}

// ------------------------------------------------------------------ state

static void copy_str(char *dst, int cap, const char *src) {
  int n = 0;
  while (n < cap - 1 && src[n]) {
    dst[n] = src[n];
    n++;
  }
  dst[n] = 0;
}

// ------------------------------------------------------------------ message history (task-62)
//
// The game's news strip (FUN_0040cb00 posts, FUN_0040d6a0 ticks) keeps one message at a time, in
// NEWS_TEXT (0x5a6d40) with screen_state (0x569498) == 1 while it shows, the subject in 0x4c2e08
// (>= 0: the golfer slot whose portrait the strip shows) and the display timer in 0x5a7144 (set to
// length / 3 + 16 at a post, counted down by the tick). A post is seen here as screen_state going to 1,
// or the timer going up while it shows (a forced message over the old one). Nothing in the game code
// is changed: the HUD only reads. What the message is about: the golfer slot when the game names one;
// else the hole whose name the text starts with (the hole hotkeys then do the camera jump).
static HudMsg g_hist[HUD_MSG_MAX];
static int g_hist_n, g_hist_seq, g_news_seq;
static int g_prev_ss, g_prev_timer;
static char g_prev_text[400];

// A golfer on the course: the record's state byte (+0x21) is neither 0 (unused) nor -1 (the "hire an
// employee" placeholder, which opens that dialog when selected).
static int golfer_ok(int slot) {
  if (slot < 0 || slot >= 152) return 0;
  int b = G8(0x5794d9 + slot * 0x100);
  return b != 0 && b != -1;
}

static int word_end(char c) { return !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'' || c == '-'); }

// The hole (1..18) whose name the message starts with (the longest name), 0 if none.
static int hole_of_text(const char *text) {
  char keep[512];
  memcpy(keep, (void *)(uintptr_t)TEXT_BUF, sizeof keep);
  int best = 0;
  size_t best_len = 0;
  for (int h = 1; h <= 18; h++) {
    *(char *)(uintptr_t)TEXT_BUF = 0;
    FUN_00407280(h);
    const char *nm = (const char *)(uintptr_t)TEXT_BUF;
    size_t n = strlen(nm);
    if (n < 2 || n >= 120 || strncmp(text, nm, n) != 0 || !word_end(text[n]) || n <= best_len) continue;
    best = h;
    best_len = n;
  }
  memcpy((void *)(uintptr_t)TEXT_BUF, keep, sizeof keep);
  return best;
}

static void note_messages(const char *date) {
  int ss = G8(SCREEN_STATE), timer = G32(0x5a7144);
  const char *text = (const char *)(uintptr_t)NEWS_TEXT;
  if (ss == 1 && text[0] && (g_prev_ss != 1 || timer > g_prev_timer || strncmp(text, g_prev_text, sizeof g_prev_text - 1) != 0)) {
    HudMsg *m;
    if (g_hist_n == HUD_MSG_MAX) {
      memmove(g_hist, g_hist + 1, sizeof g_hist[0] * (HUD_MSG_MAX - 1));
      m = &g_hist[HUD_MSG_MAX - 1];
    } else {
      m = &g_hist[g_hist_n++];
    }
    memset(m, 0, sizeof *m);
    copy_str(m->text, sizeof m->text, text);
    for (char *p = m->text; *p; p++)
      if (*p == '\n' || *p == '\r' || *p == '|') *p = ' ';
    copy_str(m->date, sizeof m->date, date);
    m->t_ms = host_now_ms();
    m->seq = g_news_seq = ++g_hist_seq;
    int subj = G32(0x4c2e08);
    if (golfer_ok(subj)) {
      m->show = HUD_SHOW_GOLFER;
      m->target = subj;
    } else if ((m->target = hole_of_text(m->text)) != 0) {
      m->show = HUD_SHOW_HOLE;
    } else if ((m->target = pv_building_of_text(m->text)) >= 0) {   // task-53: golf's "WARNING: Your ... will not be operational" names a building
      m->show = HUD_SHOW_BUILDING;
    }
    fprintf(stderr, "hud: message %d (%s %d): %s\n", m->seq, m->show == HUD_SHOW_GOLFER ? "golfer" : m->show == HUD_SHOW_HOLE ? "hole" : m->show == HUD_SHOW_BUILDING ? "building" : "no target",
            m->target, m->text);
  }
  if (ss != 1 && G32(0x5a9ccc) == 0) g_news_seq = 0;
  g_prev_ss = ss;
  g_prev_timer = timer;
  copy_str(g_prev_text, sizeof g_prev_text, text);
}

// ------------------------------------------------------------------ the player's pro (task-92)
//
// The original's People panel has a second page, the player panel (dock modes 3 and 4, FUN_004362f0 draws
// it, FUN_00436060 handles its clicks): the pro's profile disc, Practice Round, Match, Begin Tournament,
// the skill list and, while the pro is out, the five shot choices. Before the pro's first round golf runs
// the skill setup (FUN_0040f190 calls FUN_004065c0 when the skill mask is empty: 10 points plus 3 for each
// accomplishment already earned); after that every accomplishment board (FUN_0046e810) ends with "Add three
// skill points to your pro" and the skill dialog. The Frosted Mint HUD hides that panel, so the People tray's
// "Your golfer" page (ui_hud.c) shows these controls and clicks the original ones.

#define PRO_SLOT 0x5a59f8   // the pro's golfer slot while out on the course, -1 when in
#define SKILLS 0x5a5a04     // the pro's skills, 12 bytes (10 shown)
#define SKILL_MASK 0x4c2c9c

static int pro_ready(void) {   // FUN_0040f190's gate: the mask plus a bit for every skill above 0
  int mask = (uint16_t)G16(SKILL_MASK);
  for (int i = 0; i < 12; i++)
    if (G8(SKILLS + i) != 0) mask |= 1 << i;
  return mask != 0;
}

static void read_pro(HudSnap *s, int flags) {
  int slot = G32(PRO_SLOT), holes = G32(0x5685f0), busy = (flags & 0x4200000) != 0;
  s->pro_ready = pro_ready();
  s->pro_out = slot != -1;
  s->pro_practice = !s->pro_out && holes >= 2 && !busy;
  s->pro_match = !s->pro_out && (flags & 0x2000) && holes >= 2 && G32(0x4c2e14) != -1 && !busy;
  s->pro_tourney = !s->pro_out && (flags & 0x400000) && !busy;
  s->pro_shots = 0;
  if (s->pro_out && slot >= 0 && slot < 152 && s->dock_mode == 3 && G32(0x579584 + slot * 0x100) == -1) {
    int t = FUN_0040bfa0(G32(0x57958c + slot * 0x100), G32(0x5794bc + slot * 0x100));
    s->pro_shots = t >= 0 && t < 64 && G8(0x578372 + t * 0x30) >= 1 ? 1 : 2;   // a bad lie: straight and low punch only
  }
  s->pro_shot = G32(0x58f330);
  for (int i = 0; i < 10; i++) s->pro_skill[i] = G8(SKILLS + i);
  copy_str(s->pro_name, sizeof s->pro_name, (const char *)(uintptr_t)0x4d6098);
}

// [ui] pro_setup: start (default) runs the pro's creation (profile, then the skill setup) as a new career's
// course opens; original leaves it to the first Practice Round, as in 2002. SIMGOLF_PRO_SETUP overrides it
// (the test bridge launches with original so scripted new games are not held up by the dialog).
static int pro_setup_at_start(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("SIMGOLF_PRO_SETUP");
    if (!e || !*e) e = mods_get("ui.pro_setup");
    v = !(e && strcmp(e, "original") == 0);
  }
  return v;
}

void hud_clean_pages(void);

static void hud_hide_now(void) {   // a modal dialog of golf's is about to run: no HUD over it
  g_snap.visible = 0;
  g_snap.serial = ++g_serial;
  hudf_publish(&g_snap);
  hud_clean_pages();
}

static void pro_profile(void) {   // FUN_00436060 case 0, the profile disc
  G8(0x582cd0) = 0x20;
  G16(0x582d6e) = 0;
  int look = G32(0x4d60b4);
  G8(0x582cd1) = look == 0 ? 7 : (look & 7);
  FUN_004385d0(0, 0x98);
}

static void pro_action(int what) {
  fprintf(stderr, "hud: pro %s (ready %d)\n", what == HUD_PRO_SKILLS ? "skills" : what == HUD_PRO_PROFILE ? "profile" : "create",
          pro_ready());
  hud_hide_now();
  if (what == HUD_PRO_PROFILE || what == HUD_PRO_CREATE) pro_profile();
  if (what == HUD_PRO_SKILLS || what == HUD_PRO_CREATE) {
    if (!pro_ready()) {
      FUN_004065c0();   // the first setup, exactly as before the first round
    } else {            // the skills with an OK button (no points to spend)
      G32(0x4c2e08) = -1;
      FUN_0045f0f0((char *)(uintptr_t)0x4d6098, (uint16_t)G16(SKILL_MASK), 0, -1, 200);
    }
  }
  fprintf(stderr, "hud: pro done (ready %d, skills", pro_ready());
  for (int i = 0; i < 10; i++) fprintf(stderr, " %d", G8(SKILLS + i));
  fprintf(stderr, ")\n");
}

// A new career's course: the pro is created once the course view has been up a moment (no hole built yet,
// not Sandbox, skills not set up). Once per course: a new game re-arms it.
static int g_create_wait, g_create_done, g_create_tick;
static void pro_create_tick(const HudSnap *s) {
  int tick = G32(0x834170);
  if (tick < g_create_tick) g_create_done = 0;   // the clock went back: another game
  g_create_tick = tick;
  if (!pro_setup_at_start() || s->sandbox || s->pro_ready || s->next_hole >= 2) {
    g_create_wait = 0;
    return;
  }
  if (g_create_done || !s->visible) return;
  if (++g_create_wait < 45) return;
  g_create_done = 1;
  pro_action(HUD_PRO_CREATE);
}

static void read_state(HudSnap *s) {
  int flags = G32(FLAGS);
  s->canvas_w = win_screen_w;
  s->canvas_h = win_screen_h;
  s->money = G32(0x571fd4) * 100;
  s->sandbox = (flags & 0x1000000) != 0;
  s->cheat = (flags & 0x20000000) != 0;
  s->fun = G32(0x59ae78);
  s->skill_x100 = G32(0x541cd8);
  s->waiting = G32(0x56d1b0);
  int golfers = 0;
  for (int i = 0; i < 152; i++) {
    signed char hole = (signed char)G8(0x5794d9 + i * 0x100);
    if (hole > 0) golfers++;
  }
  s->golfers = golfers;
  s->dock_mode = G32(DOCK_MODE);
  s->shape = G32(0x5a34f0);
  s->amenities_on = G32(0x55e924);
  s->elevation_on = G32(0x55e928);
  s->employee_overlay = G32(0x561254);
  s->terrain_tool = G32(0x4c2848);
  s->build_tool = G32(0x4c2854);
  s->elevation_tool = G32(0x542f20);
  s->elev_edit = (flags & 2) != 0;
  s->paused = (flags & 4) != 0;
  s->fast = G32(0x59b04c) != 0;
  s->zoom = G32(0x5a34e8);   // the zoom saved around the HUD draws (the live one is forced to 4 there)
  s->rotation = G32(0x5685f4);
  s->theme = G32(0x5a34e0);
  s->unlock = G32(0x5a6364);
  s->next_hole = G32(0x5685f0);
  for (int i = 0; i < 9; i++) {
    int id = i + 6;
    s->building_level[i] = G32(0x5a8c38 + id * 4);
    // 0x4c2728 (stride 20: name, footprint +0x10, cost +0x12 in $100), entry k is tool k + 6
    const char *nm = (const char *)(uintptr_t)(0x4c2728 + i * 20);
    copy_str(s->building_name[i], sizeof s->building_name[i], nm);
    int cost = G16(0x4c2728 + i * 20 + 0x12);
    s->building_cost[i] = cost > 0 ? cost * (s->building_level[i] + 2) / 2 * 100 : -1;
  }
  for (int i = 0; i < 16; i++) {
    int id = (signed char)G8(0x4c79a0 + i);
    if (id < 0 || id > 40) {
      s->terrain_name[i][0] = 0;
      s->terrain_cost[i] = -1;
      continue;
    }
    copy_str(s->terrain_name[i], sizeof s->terrain_name[i], (const char *)(uintptr_t)(0x578350 + id * 0x30));
    if (s->terrain_name[i][0] >= 'a' && s->terrain_name[i][0] <= 'z') s->terrain_name[i][0] -= 32;   // "tees" -> "Tees"
    s->terrain_cost[i] = -1;
  }
  int ss = G8(SCREEN_STATE);
  s->news_on = ss == 1 || G32(0x5a9ccc) != 0;
  copy_str(s->news, sizeof s->news, (const char *)(uintptr_t)NEWS_TEXT);
  for (char *p = s->news; *p; p++)
    if (*p == '\n' || *p == '\r' || *p == '|') *p = ' ';
  s->visible = ss < 2;
  s->capsule_visible = G32(0x53df54) == 0 || gw_native_card();   // the native golfer card (task-31) leaves the top bar alone
  // course name and date, through the game's own formatters (text_buf is scratch)
  char keep[512];
  memcpy(keep, (void *)(uintptr_t)TEXT_BUF, sizeof keep);
  *(char *)(uintptr_t)TEXT_BUF = 0;
  FUN_0040daa0(0);
  copy_str(s->course, sizeof s->course, (const char *)(uintptr_t)TEXT_BUF);
  *(char *)(uintptr_t)TEXT_BUF = 0;
  FUN_0040d7b0(G32(0x834170));
  copy_str(s->date, sizeof s->date, (const char *)(uintptr_t)TEXT_BUF);
  memcpy((void *)(uintptr_t)TEXT_BUF, keep, sizeof keep);
  note_messages(s->date);
  s->news_seq = g_news_seq;
  s->msg_n = g_hist_n;
  memcpy(s->msgs, g_hist, sizeof g_hist[0] * (size_t)g_hist_n);
  read_pro(s, flags);
}

// ------------------------------------------------------------------ actions

// (x, y): a point of the original 800x600 layout of the wheel and trays,
// which task-3 B4 draws at the bottom of a taller canvas (HD_UI_DY down).
static void inject(int x, int y) {
  fprintf(stderr, "hud: click (%d,%d) of the original layout\n", x, y);
  y += win_screen_h - 600;
  g_inj = 1;
  g_inj_x = x;
  g_inj_y = y;
  g_inj_frames = 0;
  hudf_set_injecting(1);
  FUN_004657e0(x, y);
}

// One step towards a goal tray; returns 1 when it is reached.
static int goal_step(int goal) {
  int mode = G32(DOCK_MODE), amen = G32(0x55e924), elev = G32(0x55e928);
  switch (goal) {
    case HUD_GOAL_COURSE:
      if (mode != 0) return inject(44, 474), 0;
      if (amen) return inject(269, 500), 0;
      return 1;
    case HUD_GOAL_EXTRAS:
      if (mode != 0) return inject(44, 474), 0;
      if (!amen) return inject(237, 525), 0;
      return 1;
    case HUD_GOAL_BUILDINGS:
      if (mode != 1) return inject(117, 496), 0;
      if (elev) return inject(270, 499), 0;
      return 1;
    case HUD_GOAL_LANDSCAPE:
      if (mode != 1) return inject(117, 496), 0;
      if (!elev) return inject(236, 526), 0;
      return 1;
    case HUD_GOAL_PEOPLE:
      if (mode < 2 || mode > 4) return inject(176, 537), 0;
      return 1;
    case HUD_GOAL_CLOSE:
      if (mode == 0) return inject(44, 474), 0;
      if (mode == 1) return inject(117, 496), 0;
      if (mode >= 2 && mode <= 4) return inject(176, 537), 0;
      return 1;
  }
  return 1;
}

static void terrain_slot_point(int slot, int *x, int *y) {
  int px = G16(0x4c79b4 + slot * 4), py = G16(0x4c79b6 + slot * 4);
  int h = G32(0x570cd4 + slot * 0xb0);
  *x = px + 31;
  *y = py + h - 32;   // FUN_00432f90: y in [py + h - 54, py + h - 10)
}

static int g_show_log;   // frames until the result of a HUD_ACT_SHOW is logged
static int g_show_golfer = -1, g_show_wait;   // a golfer to show once the dock is closed (the chip strip's precondition)

static void show_golfer(int slot) {
  // what a click on the golfer chip strip does (dock closed): select the golfer, camera to their tile
  G32(0x53df54) = slot + 1;
  G8(SCREEN_STATE) = 0;
  G32(0x4c2ba0) = G32(0x5794b8 + slot * 0x100) >> 10;
  G32(0x4c2ba4) = G32(0x5794bc + slot * 0x100) >> 10;
  fprintf(stderr, "hud: show golfer %d\n", slot);
  g_show_log = 4;
}

static void run_actions(void) {
  if (g_show_log && --g_show_log == 0)
    fprintf(stderr, "hud: after show: selection %d, camera tile %d,%d\n", G32(0x53df54), G32(0x4c2ba0), G32(0x4c2ba4));
  if (g_inj) {   // a click of ours is still on its way (or was swallowed)
    if (++g_inj_frames < 4) return;
    g_inj = 0;
    hudf_set_injecting(0);
  }
  if (g_show_golfer >= 0 && !g_goal_on) {
    if (G32(DOCK_MODE) == 5) {
      show_golfer(g_show_golfer);
      g_show_golfer = -1;
    } else if (++g_show_wait > 20) {
      g_show_golfer = -1;
    }
    return;
  }
  if (g_goal_on) {
    if (++g_goal_steps > 6) {
      g_goal_on = 0;
    } else if (goal_step(g_goal.goal)) {
      g_goal_on = 0;
      if (g_goal.x <= -2) {
        int x, y;
        terrain_slot_point(-2 - g_goal.x, &x, &y);
        inject(x, y);
      } else if (g_goal.x >= 0) {
        inject(g_goal.x, g_goal.y);
      }
    }
    return;
  }
  HudAction a;
  if (!hudf_take_action(&a)) return;
  switch (a.kind) {
    case HUD_ACT_CLICK: inject(a.x, a.y); break;
    case HUD_ACT_KEY: G32(0x822b9c) = a.key; break;
    case HUD_ACT_VKEY: G32(0x822b98) = a.key; break;
    case HUD_ACT_SHOW:
      if (a.x == HUD_SHOW_GOLFER && golfer_ok(a.y)) {
        if (G32(DOCK_MODE) == 5) {
          show_golfer(a.y);
        } else {   // close the dock first (the chip strip only works then), then select
          g_show_golfer = a.y;
          g_show_wait = 0;
          g_goal = (HudAction){HUD_ACT_GOAL, -1, -1, HUD_GOAL_CLOSE, 0};
          g_goal_on = 1;
          g_goal_steps = 0;
        }
      } else if (a.x == HUD_SHOW_HOLE && a.y >= 1 && a.y <= 18) {
        // the hole hotkeys: 1..9, shift+1..9 for 10..18 (select the hole, camera to its tee and green)
        G32(0x822b9c) = a.y <= 9 ? '0' + a.y : "!@#$%^&*("[a.y - 10];
        fprintf(stderr, "hud: show hole %d\n", a.y);
        g_show_log = 4;
      } else if (a.x == HUD_SHOW_BUILDING) {
        pv_show_building(a.y);
        G8(SCREEN_STATE) = 0;
        g_show_log = 4;
      }
      break;
    case HUD_ACT_PRO: pro_action(a.x); break;
    case HUD_ACT_GOAL:
      g_goal = a;
      g_goal_on = 1;
      g_goal_steps = 0;
      run_actions();
      break;
  }
}

// task-27: a modal dialog opens while the old HUD has just drawn this frame (the hire panel is called from the
// employee tray's draw): put the course back under the old HUD so the dialog keeps a clean picture.
void hud_clean_pages(void) {
  if (!g_on) return;
  for (int i = 0; i < PAGES; i++) copy_rects(i, 0);
  hide_art();
}

void hud_restore(void) {
  if (!g_on) return;
  for (int i = 0; i < PAGES; i++)
    if (copy_rects(i, 2)) {
      g_target = i;
      break;
    }
  int theme = G32(0x5a34e0);
  if (g_target >= 0 && (theme != g_art_theme || ++g_art_frames > 600)) {
    g_art_theme = theme;
    g_art_frames = 0;
    snap_art();
  }
  for (int i = 0; i < PAGES; i++) copy_rects(i, 0);
  hide_art();
  HudSnap *s = &g_snap;
  read_state(s);
  s->valid = 1;
  s->serial = ++g_serial;
  s->art_serial = g_art_serial;
  hudf_publish(s);
  // one line whenever what the HUD shows changes (tests read these)
  static char last[160];
  char now[160];
  snprintf(now, sizeof now, "dock %d amenities %d elevation %d staff %d terrain %d build %d elev_tool %d elev_edit %d paused %d zoom %d rot %d shape %d pro %d%d%d",
           s->dock_mode, s->amenities_on, s->elevation_on, s->employee_overlay, s->terrain_tool, s->build_tool,
           s->elevation_tool, s->elev_edit, s->paused, G32(0x4c2844), s->rotation, s->shape, s->pro_ready, s->pro_out, s->pro_practice);
  if (strcmp(now, last)) {
    fprintf(stderr, "hud: state %s\n", now);
    memcpy(last, now, sizeof last);
  }
  run_actions();
  pro_create_tick(s);
}

