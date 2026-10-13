// Frosted Mint shot aim arrows, render side (simgolf mod, task-115), native build only. One UI layer of the
// presenter (hd_native.h) under the HUD; it draws the AimNote the game thread attached to the frame (third note
// channel), pinned like the cart path view's tiles.
#pragma clang diagnostic error "-Wimplicit-function-declaration"

#include "port/aim_frosted.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mods/mods.h"
#include "port/hd_native.h"
#include "port/host.h"
#include "port/runtime.h"
#include "port/ui/ui_aim.h"

static UiCtx *g_ui;
static int g_reg, g_failed;
static volatile int g_want;

static void load_tokens(UiTokens *t) {
  char base[1024];
  snprintf(base, sizeof base, "%s", port_host_path("/ui/frosted_mint.toml"));
  static char paths[16][1024];
  const char *ov[16];
  int n = 0;
  for (int i = 0; i < mods_count() && n < 16; i++) {
    const char *dir = mods_dir(mods_id_at(i));
    if (!dir) continue;
    snprintf(paths[n], sizeof paths[n], "%s/ui/frosted_mint.toml", dir);
    FILE *f = fopen(paths[n], "rb");
    if (!f) continue;
    fclose(f);
    ov[n] = paths[n];
    n++;
  }
  ui_tokens_load_all(t, base, ov, n);
}

static float ui_scale_now(int h) {
  double s = mods_get_num("ui.scale", 0);
  if (s >= 0.5 && s <= 4) return (float)s;
  return ui_scale_for_height(h);
}

static void draw(const HdUiFrame *f, void *user) {
  (void)user;
  const AimNote *n = (const AimNote *)f->note3;
  if (!g_want || g_failed || !f->course || !f->layered || !n || f->note3_size != (int)sizeof(AimNote) || n->magic != AIM_MAGIC) {
    if (g_ui) ui_skip_frame(g_ui);
    return;
  }
  SDL_Renderer *r = f->renderer;
  if (!g_ui) {
    UiTokens t;
    load_tokens(&t);
    char fonts[1024];
    snprintf(fonts, sizeof fonts, "%s", port_host_path("/fonts/plus-jakarta-sans"));
    g_ui = ui_create(r, &t, fonts);
    if (!g_ui) {
      g_failed = 1;
      return;
    }
    fprintf(stderr, "ui: Frosted Mint shot aim layer on\n");
  }
  int w = 0, h = 0;
  if (f->target) SDL_QueryTexture(f->target, NULL, NULL, &w, &h);
  else SDL_GetRendererOutputSize(r, &w, &h);
  if (w <= 0) return;
  PvMap m;
  m.view_x = (float)f->view.x;
  m.view_y = (float)f->view.y;
  m.px = f->px > 0 ? f->px : 1;
  m.slide_x = f->slide_x;
  m.slide_y = f->slide_y;
  m.zoom_on = f->zoom_on;
  m.zoom_s = f->zoom_s;
  m.zoom_x = f->zoom_x;
  m.zoom_y = f->zoom_y;
  // the test line (tests/native/aim_tee_rotate.mjs): the shot, the points, and how far the middle of the arc bends off the chord
  static uint32_t logged;
  if (n->serial != logged && n->n >= 3 && getenv("SIMGOLF_AIM_LOG")) {
    logged = n->serial;
    float cx = (float)(n->ex[n->n - 1] - n->ex[0]), cy = (float)(n->ey[n->n - 1] - n->ey[0]);
    float len = sqrtf(cx * cx + cy * cy), bend = 0;
    int mid = n->n / 2;
    if (len > 1) bend = ((float)(n->ex[mid] - n->ex[0]) * cy - (float)(n->ey[mid] - n->ey[0]) * cx) / len;
    fprintf(stderr, "aim: shot %d points %d ground %d start %d,%d end %d,%d ground end %d,%d bend %.1f\n", n->shot, n->n, n->ng, n->ex[0], n->ey[0],
            n->ex[n->n - 1], n->ey[n->n - 1], n->ng ? n->gx[n->ng - 1] : 0, n->ng ? n->gy[n->ng - 1] : 0, bend);
  }
  ui_begin(g_ui, f->target, w, h, ui_scale_now(h), f->target, host_now_ms());
  ui_aim_draw(g_ui, n, &m, f->now_ms);
  ui_end(g_ui);
}

static void refresh_switch(void) {
  static unsigned seen = ~0u;
  unsigned ser = mods_serial();
  if (ser != seen) {
    seen = ser;
    const char *v = mods_get("ui.aim");
    g_want = v && (!strcasecmp(v, "frosted_mint") || !strcasecmp(v, "frosted-mint"));
  }
}

int aimf_enabled(void) {
  refresh_switch();
  if (g_want && !g_reg && !g_failed) {
    g_reg = hd_ui_add_layer(draw, NULL);
    if (!g_reg) g_failed = 1;
  }
  return g_want && g_reg && !g_failed;
}
