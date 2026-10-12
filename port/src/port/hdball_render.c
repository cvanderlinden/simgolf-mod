// Round HD golf ball, render-thread half (task-41, visual physics of task-43a).
// See hdball.h for the split.
//
// Every game frame (about 11 a second) the game thread records where each
// ball is. The presenter runs at the display rate, so a ball drawn where golf
// has it would hop 11 times a second. Instead the last four positions of
// every ball are kept with the time golf finished each frame, and the ball is
// drawn at (now - lag game frames) on a smooth path through them: a monotone
// cubic (Fritsch-Carlson / Butland slopes) per coordinate. Through golf's own
// samples exactly, no overshoot (a ball that stops never creeps back, an apex
// is not exceeded), continuous speed in between. The ball therefore reaches
// every position golf computed, in golf's order, one game frame (default)
// after golf did, and the last one exactly: same landing spot, same stop.
//
// Looks: golf's flight is flat (the lift of a 90 yard shot is a few pixels),
// so the height is drawn lift_scale times higher; the ground point does not
// change. The ball is a shaded sphere with a light from the top left, a rim,
// a specular spot and faint dimples when large, rendered at the exact
// on-screen size; its soft shadow lies on the ground at golf's ground point,
// shifted sideways with height, wider and fainter the higher the ball.
#define _GNU_SOURCE
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/hdball_render.h"
#include "port/host.h"
#include "port/runtime.h"

// ------------------------------------------------------------------ tuning (data/ui/ball.txt)

static struct {
  double radius, radius_min_px, radius_max_px, lift_scale, lag, rest_lift;
  double shadow_size, shadow_alpha, shadow_flat, shadow_sun;
  double lx, ly, lz, ambient, spec;
  int dimples;
} C = {2.0, 2.2, 48, 1.0, 1.0, 0.85, 2.4, 0.55, 0.50, 0.16, -0.42, -0.58, 0.70, 0.74, 0.55, 1};
static int g_cfg_loaded;

static void load_cfg(void) {
  if (g_cfg_loaded) return;
  g_cfg_loaded = 1;
  FILE *f = fopen(port_host_path("/ui/ball.txt"), "r");
  if (!f) {
    fprintf(stderr, "ball: ui/ball.txt not found, built-in numbers\n");
    return;
  }
  char line[256];
  int n = 0;
  while (fgets(line, sizeof line, f)) {
    char *h = strchr(line, '#');
    if (h) *h = 0;
    char key[48];
    double a = 0, b = 0, c = 0;
    int got = sscanf(line, "%47s %lf %lf %lf", key, &a, &b, &c);
    if (got < 2) continue;
    n++;
#define K(name, field) else if (!strcmp(key, name)) C.field = a
    if (0) {}
    K("radius", radius);
    K("radius_min_px", radius_min_px);
    K("radius_max_px", radius_max_px);
    K("lift_scale", lift_scale);
    K("lag", lag);
    K("rest_lift", rest_lift);
    K("shadow_size", shadow_size);
    K("shadow_alpha", shadow_alpha);
    K("shadow_flat", shadow_flat);
    K("shadow_sun", shadow_sun);
    K("ambient", ambient);
    K("spec", spec);
    else if (!strcmp(key, "dimples")) C.dimples = (int)a;
    else if (!strcmp(key, "light") && got == 4) C.lx = a, C.ly = b, C.lz = c;
    else n--, fprintf(stderr, "ball: ui/ball.txt: unknown key '%s'\n", key);
#undef K
  }
  fclose(f);
  double l = sqrt(C.lx * C.lx + C.ly * C.ly + C.lz * C.lz);
  if (l > 0) C.lx /= l, C.ly /= l, C.lz /= l;
  fprintf(stderr, "ball: ui/ball.txt, %d values (radius %.2f canvas px at zoom 4, lift x%.2f, lag %.2f frames)\n", n, C.radius,
          C.lift_scale, C.lag);
}

// ------------------------------------------------------------------ history

enum { M_GX, M_GY, M_LIFT, M_OCCB, M_OCCS, M_WX, M_WY, M_WZ, NCOMP };
#define NS 4   // samples kept per ball

typedef struct {
  double t;
  double v[NCOMP];
} Smp;
typedef struct {
  int n;
  Smp s[NS];
  int in_frame;   // present in the last frame ingested
} Hist;

static Hist g_h[HB_MAX];
static HdUiCamera g_cam;
static int g_have_cam, g_zoom2;
static double g_last_t, g_interval = 87;
static double g_path_t;   // the path's own time of the newest frame (see hdball_ingest)
static FILE *g_log;
static int g_log_tried;

static void log_open(void) {
  if (g_log_tried) return;
  g_log_tried = 1;
  const char *p = getenv("SIMGOLF_BALL_LOG");
  if (p && *p) {
    g_log = fopen(p, "w");
    if (g_log) fprintf(g_log, "row,a,b,slot,c,d,e,f,g,h,i,j,k\n");
  }
}

static void clear_all(void) {
  for (int i = 0; i < HB_MAX; i++) g_h[i].n = 0, g_h[i].in_frame = 0;
  g_have_cam = 0;
}

void hdball_ingest(const HdBallFrame *f, const HdUiCamera *cam, int course_layered, double now_ms) {
  log_open();
  if (!course_layered || !f->on) {
    if (g_log) fprintf(g_log, "X,%u,%.3f,%d,%d\n", f->frame, now_ms, course_layered, f->on);   // history dropped: not a layered course frame
    clear_all();
    return;
  }
  double dx = 0, dy = 0;
  if (g_have_cam) {
    if (cam->zoom != g_cam.zoom || cam->rot != g_cam.rot || f->zoom2 != g_zoom2) {
      clear_all();
    } else {
      // where a point of the previous frame's canvas is in this frame's (the presenter's slide uses the same)
      dx = (cam->x - g_cam.x) * cam->ax + (cam->y - g_cam.y) * cam->bx + (cam->ox - g_cam.ox);
      dy = (cam->x - g_cam.x) * cam->ay + (cam->y - g_cam.y) * cam->by + (cam->oy - g_cam.oy);
    }
  }
  g_cam = *cam;
  g_have_cam = 1;
  g_zoom2 = f->zoom2;
  if (g_log) fprintf(g_log, "C,%u,%.3f,%.2f,%.2f\n", f->frame, now_ms, dx, dy);   // the camera shift applied to older positions
  if (g_last_t > 0) {
    double iv = f->t_ms - g_last_t;
    if (iv > 15 && iv < 400) g_interval = g_interval * 0.8 + iv * 0.2;
  }
  g_last_t = f->t_ms;
  // The path's time for this frame: when golf finished it, but never closer than
  // half a game frame to the one before. golf catches up after a slow frame by
  // running two or three at once; played back at that pace the ball would hop.
  // (Such samples sit up to half a frame late; the next on-time frame is back on the clock.)
  double pt = f->t_ms;
  if (g_path_t > 0 && pt < g_path_t + 0.5 * g_interval) pt = g_path_t + 0.5 * g_interval;
  if (g_path_t > 0 && pt > f->t_ms + 2 * g_interval) pt = f->t_ms;
  g_path_t = pt;
  for (int i = 0; i < HB_MAX; i++) g_h[i].in_frame = 0;
  for (int i = 0; i < f->n; i++) {
    const HdBallRec *r = &f->b[i];
    Hist *h = &g_h[r->slot];
    for (int k = 0; k < h->n; k++) {
      h->s[k].v[M_GX] += dx;
      h->s[k].v[M_GY] += dy;
    }
    if (h->n) {
      const Smp *l = &h->s[h->n - 1];
      // a ball that jumps (picked up, put on the next tee, a drop) is a new ball, not a very fast one
      if (fabs(r->wx - l->v[M_WX]) + fabs(r->wy - l->v[M_WY]) > 2600 || fabs(r->wz - l->v[M_WZ]) > 1500) h->n = 0;
    }
    if (h->n == NS) {
      memmove(&h->s[0], &h->s[1], sizeof(Smp) * (NS - 1));
      h->n--;
    }
    Smp *s = &h->s[h->n++];
    s->t = pt;
    s->v[M_GX] = r->gx;
    s->v[M_GY] = r->gy;
    s->v[M_LIFT] = r->lift;
    s->v[M_OCCB] = r->occ_ball;
    s->v[M_OCCS] = r->occ_shadow;
    s->v[M_WX] = r->wx;
    s->v[M_WY] = r->wy;
    s->v[M_WZ] = r->wz;
    h->in_frame = 1;
    if (g_log)
      fprintf(g_log, "G,%u,%.3f,%d,%d,%d,%d,%d,%.1f,%.1f,%.1f,%.2f,%.2f,%.3f\n", f->frame, f->t_ms, r->slot, r->kind, r->wx, r->wy,
              r->wz, r->gx, r->gy, r->lift, r->occ_ball, r->occ_shadow, pt);
  }
  for (int i = 0; i < HB_MAX; i++)
    if (!g_h[i].in_frame) g_h[i].n = 0;   // gone from the course
  if (g_log) fflush(g_log);
}

// ------------------------------------------------------------------ the path

static double end_slope(double h0, double h1, double d0, double d1) {
  double s = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
  if (s * d0 <= 0) s = 0;
  else if (d0 * d1 <= 0 && fabs(s) > 3 * fabs(d0)) s = 3 * d0;
  return s;
}

// Monotone piecewise cubic Hermite through (t[i], y[i]), evaluated at x; held
// at the ends. Equal to the samples at the samples; never beyond neighbours.
static double pchip(int m, const double *t, const double *y, double x, int *seg) {
  *seg = 0;
  if (m == 1 || x <= t[0]) return y[0];
  if (x >= t[m - 1]) {
    *seg = m - 1;
    return y[m - 1];
  }
  int i = 0;
  while (i < m - 2 && x > t[i + 1]) i++;
  *seg = i;
  double h[NS], d[NS], s[NS];
  for (int k = 0; k < m - 1; k++) {
    h[k] = t[k + 1] - t[k];
    d[k] = (y[k + 1] - y[k]) / h[k];
  }
  if (m == 2) {
    s[0] = s[1] = d[0];
  } else {
    for (int k = 1; k < m - 1; k++) {
      if (d[k - 1] * d[k] <= 0) s[k] = 0;
      else {
        double w1 = 2 * h[k] + h[k - 1], w2 = h[k] + 2 * h[k - 1];
        s[k] = (w1 + w2) / (w1 / d[k - 1] + w2 / d[k]);
      }
    }
    s[0] = end_slope(h[0], h[1], d[0], d[1]);
    s[m - 1] = end_slope(h[m - 2], h[m - 3], d[m - 2], d[m - 3]);
  }
  double u = (x - t[i]) / h[i], u2 = u * u, u3 = u2 * u;
  return (2 * u3 - 3 * u2 + 1) * y[i] + (u3 - 2 * u2 + u) * h[i] * s[i] + (-2 * u3 + 3 * u2) * y[i + 1] +
         (u3 - u2) * h[i] * s[i + 1];
}

static void eval(const Hist *h, double td, double *out, int *seg) {
  double t[NS], y[NS];
  for (int k = 0; k < h->n; k++) t[k] = h->s[k].t;
  for (int c = 0; c < NCOMP; c++) {
    for (int k = 0; k < h->n; k++) y[k] = h->s[k].v[c];
    out[c] = pchip(h->n, t, y, td, seg);
  }
}

// ------------------------------------------------------------------ pictures

static double smooth(double e0, double e1, double x) {
  double t = (x - e0) / (e1 - e0);
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return t * t * (3 - 2 * t);
}

#define MAX_BALLTEX 24
static struct {
  int key;   // radius in quarter pixels
  SDL_Texture *tex;
  int d;
} g_bt[MAX_BALLTEX];
static int g_nbt;
static SDL_Texture *g_shadow;
static SDL_Renderer *g_renderer;

// The ball at radius r (target pixels) as a (2r+4)-px square, straight alpha.
static SDL_Texture *make_ball(SDL_Renderer *rd, double r, int *dout) {
  int D = (int)ceil(2 * r) + 4;
  uint32_t *px = calloc((size_t)D * D, 4);
  double c = D / 2.0;
  for (int j = 0; j < D; j++)
    for (int i = 0; i < D; i++) {
      double X = i + 0.5 - c, Y = j + 0.5 - c;
      double d = sqrt(X * X + Y * Y);
      double cover = r - d + 0.5;
      if (cover <= 0) continue;
      if (cover > 1) cover = 1;
      double nx = X / r, ny = Y / r;
      double nn = nx * nx + ny * ny;
      if (nn > 1) nx /= sqrt(nn), ny /= sqrt(nn), nn = 1;
      double nz = sqrt(1 - nn);
      double diff = nx * C.lx + ny * C.ly + nz * C.lz;
      if (diff < 0) diff = 0;
      // base: warm white; light from the top left, a cool green bounce from the grass below, a darker rim
      double shade = C.ambient + (1 - C.ambient) * pow(diff, 0.85);
      double lr = shade, lg = shade, lb = shade;
      double bounce = ny > 0 ? ny * 0.10 * (1 - nz) : 0;
      lr += bounce * 0.55, lg += bounce * 0.85, lb += bounce * 0.55;
      double rim = 0.86 + 0.14 * pow(nz, 0.45);
      lr *= rim, lg *= rim, lb *= rim;
      if (C.dimples && r >= 5.5) {
        // stereographic hex lattice of shallow dimples, fading to the edge
        double u = nx / (1 + nz), v = ny / (1 + nz), sp = 0.30;
        double a = u / sp, b = v / (sp * 0.866);
        double row = floor(b + 0.5);
        double off = ((long)row & 1) ? 0.5 : 0.0;
        double fx = a - off - floor(a - off + 0.5), fy = (b - row) * 0.866;
        double dist = sqrt(fx * fx + fy * fy);
        double dimple = 1 - smooth(0.28, 0.50, dist);
        double k = 1 - 0.12 * dimple * pow(nz, 0.8);
        lr *= k, lg *= k, lb *= k;
      }
      // specular spot
      double rdotv = 2 * (nx * C.lx + ny * C.ly + nz * C.lz) * nz - C.lz;
      double sp = rdotv > 0 ? pow(rdotv, 38) * C.spec : 0;
      lr += sp, lg += sp, lb += sp;
      double br = 0.985, bg = 0.97, bb = 0.93;   // paint
      int R = (int)(fmin(1.0, lr * br) * 255 + 0.5), Gc = (int)(fmin(1.0, lg * bg) * 255 + 0.5),
          B = (int)(fmin(1.0, lb * bb) * 255 + 0.5);
      int A = (int)(cover * 255 + 0.5);
      px[j * D + i] = (uint32_t)A << 24 | (uint32_t)R << 16 | (uint32_t)Gc << 8 | (uint32_t)B;
    }
  SDL_Texture *t = SDL_CreateTexture(rd, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, D, D);
  if (t) {
    SDL_UpdateTexture(t, NULL, px, D * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
  }
  free(px);
  *dout = D;
  return t;
}

static SDL_Texture *ball_tex(SDL_Renderer *rd, double r, int *d) {
  int key = (int)lround(r * 4);
  for (int i = 0; i < g_nbt; i++)
    if (g_bt[i].key == key) {
      *d = g_bt[i].d;
      return g_bt[i].tex;
    }
  if (g_nbt == MAX_BALLTEX) {   // sizes change only with the zoom; start over
    for (int i = 0; i < g_nbt; i++) SDL_DestroyTexture(g_bt[i].tex);
    g_nbt = 0;
  }
  SDL_Texture *t = make_ball(rd, key / 4.0, d);
  g_bt[g_nbt].key = key;
  g_bt[g_nbt].tex = t;
  g_bt[g_nbt].d = *d;
  g_nbt++;
  return t;
}

static SDL_Texture *shadow_tex(SDL_Renderer *rd) {
  if (g_shadow) return g_shadow;
  enum { S = 64 };
  uint32_t px[S * S];
  for (int j = 0; j < S; j++)
    for (int i = 0; i < S; i++) {
      double X = (i + 0.5 - S / 2.0) / (S / 2.0), Y = (j + 0.5 - S / 2.0) / (S / 2.0);
      double d = sqrt(X * X + Y * Y);
      double a = 1 - smooth(0.0, 1.0, d);
      a = pow(a, 1.0);
      px[j * S + i] = (uint32_t)(a * 255 + 0.5) << 24 | 0x10180c;
    }
  g_shadow = SDL_CreateTexture(rd, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, S, S);
  if (g_shadow) {
    SDL_UpdateTexture(g_shadow, NULL, px, S * 4);
    SDL_SetTextureBlendMode(g_shadow, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(g_shadow, SDL_ScaleModeLinear);
  }
  return g_shadow;
}

// ------------------------------------------------------------------ drawing

typedef struct {
  double gx, gy, lift, occb, occs;   // canvas px, interpolated
  double speed;                      // canvas px per second, ground and height together
  int slot;
} Draw;

// ------------------------------------------------------------------ capture (evidence only)
//
// SIMGOLF_BALL_CAPTURE="dir=PATH;w=640;h=360;every=4;max=900" saves a w x h crop
// around the fastest moving ball (the course and the ball, no UI) as BMPs
// c<clip>_f<n>.bmp every `every` presented frames, with frames.csv (clip, frame,
// time). A clip starts when a ball moves and ends 0.7 s after the last one
// stopped. Used for the GIFs of a drive and a putt (tools/ball_gif.py).
static struct {
  int tried, on, w, h, every, max, saved, clip, frame, tick;
  char dir[260];
  double cx, cy, idle_until;
  int active;
  FILE *csv;
} g_cap;

static void cap_init(void) {
  if (g_cap.tried) return;
  g_cap.tried = 1;
  const char *e = getenv("SIMGOLF_BALL_CAPTURE");
  if (!e || !*e) return;
  char buf[600];
  snprintf(buf, sizeof buf, "%s", e);
  g_cap.w = 640, g_cap.h = 360, g_cap.every = 4, g_cap.max = 900;
  for (char *t = strtok(buf, ";"); t; t = strtok(NULL, ";")) {
    if (!strncmp(t, "dir=", 4)) snprintf(g_cap.dir, sizeof g_cap.dir, "%s", t + 4);
    else if (!strncmp(t, "w=", 2)) g_cap.w = atoi(t + 2);
    else if (!strncmp(t, "h=", 2)) g_cap.h = atoi(t + 2);
    else if (!strncmp(t, "every=", 6)) g_cap.every = atoi(t + 6);
    else if (!strncmp(t, "max=", 4)) g_cap.max = atoi(t + 4);
  }
  if (!g_cap.dir[0] || g_cap.w < 16 || g_cap.h < 16 || g_cap.every < 1) return;
  char p[300];
  snprintf(p, sizeof p, "%s/frames.csv", g_cap.dir);
  g_cap.csv = fopen(p, "w");
  if (g_cap.csv) fprintf(g_cap.csv, "clip,frame,now_ms,cx,cy\n");
  g_cap.on = g_cap.csv != NULL;
  fprintf(stderr, "ball: capture to %s, %dx%d every %d presented frames\n", g_cap.dir, g_cap.w, g_cap.h, g_cap.every);
}

static void write_bmp24(const char *path, const uint32_t *argb, int w, int h) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  int row = (w * 3 + 3) & ~3;
  uint8_t hdr[54] = {'B', 'M'};
  uint32_t size = 54 + (uint32_t)row * h;
  memcpy(hdr + 2, &size, 4);
  hdr[10] = 54;
  hdr[14] = 40;
  memcpy(hdr + 18, &w, 4);
  memcpy(hdr + 22, &h, 4);
  hdr[26] = 1;
  hdr[28] = 24;
  fwrite(hdr, 1, 54, f);
  uint8_t *line = calloc(1, row);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      uint32_t c = argb[(size_t)y * w + x];
      line[x * 3] = (uint8_t)c, line[x * 3 + 1] = (uint8_t)(c >> 8), line[x * 3 + 2] = (uint8_t)(c >> 16);
    }
    fwrite(line, 1, row, f);
  }
  free(line);
  fclose(f);
}

// after the balls are drawn: (fx, fy) the fastest ball's centre on the target, speed in canvas px/s (< 0: none)
static void cap_frame(SDL_Renderer *rd, double fx, double fy, double speed, double now_ms) {
  if (!g_cap.on || g_cap.saved >= g_cap.max) return;
  int moving = speed > 40;
  if (moving) {
    if (!g_cap.active) {
      g_cap.active = 1;
      g_cap.clip++;
      g_cap.frame = 0;
      g_cap.tick = 0;
      g_cap.cx = fx, g_cap.cy = fy;
    }
    g_cap.idle_until = now_ms + 700;
  } else if (!g_cap.active || now_ms > g_cap.idle_until) {
    g_cap.active = 0;
    return;
  }
  if (moving) {   // the window follows the ball, eased
    g_cap.cx += (fx - g_cap.cx) * 0.12;
    g_cap.cy += (fy - g_cap.cy) * 0.12;
  }
  if (g_cap.tick++ % g_cap.every) return;
  int ow = 0, oh = 0;
  SDL_Texture *t = SDL_GetRenderTarget(rd);
  if (t) SDL_QueryTexture(t, NULL, NULL, &ow, &oh);
  else SDL_GetRendererOutputSize(rd, &ow, &oh);
  int x0 = (int)lround(g_cap.cx - g_cap.w / 2.0), y0 = (int)lround(g_cap.cy - g_cap.h / 2.0);
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x0 + g_cap.w > ow) x0 = ow - g_cap.w;
  if (y0 + g_cap.h > oh) y0 = oh - g_cap.h;
  if (x0 < 0 || y0 < 0) return;
  SDL_Rect r = {x0, y0, g_cap.w, g_cap.h};
  uint32_t *px = malloc((size_t)g_cap.w * g_cap.h * 4);
  if (!px) return;
  if (!SDL_RenderReadPixels(rd, &r, SDL_PIXELFORMAT_ARGB8888, px, g_cap.w * 4)) {
    char p[320];
    snprintf(p, sizeof p, "%s/c%02d_f%04d.bmp", g_cap.dir, g_cap.clip, g_cap.frame);
    write_bmp24(p, px, g_cap.w, g_cap.h);
    fprintf(g_cap.csv, "%d,%d,%.3f,%d,%d\n", g_cap.clip, g_cap.frame, now_ms, x0, y0);
    fflush(g_cap.csv);
    g_cap.frame++;
    g_cap.saved++;
  }
  free(px);
}

static int by_depth(const void *a, const void *b) {
  double x = ((const Draw *)a)->gy, y = ((const Draw *)b)->gy;
  return x < y ? -1 : x > y;
}

void hdball_draw(SDL_Renderer *rd, int cdx, int cdy, const SDL_Rect *clip, float k, int canvas_w, int canvas_h, double now_ms) {
  load_cfg();
  if (rd != g_renderer) {   // a new renderer: textures belong to the old one
    g_renderer = rd;
    g_nbt = 0;
    g_shadow = NULL;
  }
  double td = now_ms - C.lag * g_interval;
  Draw list[HB_MAX];
  int n = 0;
  for (int i = 0; i < HB_MAX; i++) {
    const Hist *h = &g_h[i];
    if (!h->n) continue;
    double v[NCOMP];
    int seg;
    eval(h, td, v, &seg);
    list[n].gx = v[M_GX];
    list[n].gy = v[M_GY];
    list[n].lift = v[M_LIFT] < 0 ? 0 : v[M_LIFT];
    list[n].occb = v[M_OCCB];
    list[n].occs = v[M_OCCS];
    list[n].slot = i;
    {
      double vb[NCOMP];
      int sg;
      eval(h, td - 40, vb, &sg);
      double dx = v[M_GX] - vb[M_GX], dy = (v[M_GY] - v[M_LIFT]) - (vb[M_GY] - vb[M_LIFT]);
      list[n].speed = hypot(dx, dy) * 1000.0 / 40.0;
    }
    if (g_log)
      fprintf(g_log, "D,%.3f,%.3f,%d,%d,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%d\n", now_ms, td, i, h->n, v[M_GX], v[M_GY], v[M_LIFT], v[M_WX],
              v[M_WY], v[M_WZ], seg);
    n++;
  }
  if (!n) return;
  qsort(list, n, sizeof list[0], by_depth);
  int s2 = g_zoom2 ? 2 : 1;
  double zoom = g_cam.zoom >= 1 && g_cam.zoom <= 8 ? g_cam.zoom : 4;
  double rc = C.radius * zoom / 4 * s2;   // radius, canvas px
  double rs = rc * k;
  if (rs < C.radius_min_px) rs = C.radius_min_px;
  if (rs > C.radius_max_px) rs = C.radius_max_px;
  double rcanvas = rs / k;
  SDL_Texture *sh = shadow_tex(rd);
  int D = 0;
  SDL_Texture *bt = ball_tex(rd, rs, &D);
  SDL_RenderSetClipRect(rd, clip);
  cap_init();
  double cap_x = 0, cap_y = 0, cap_speed = -1;
  for (int i = 0; i < n; i++) {
    double gx = list[i].gx, gy = list[i].gy;
    if (g_zoom2) {
      gx = (gx - canvas_w / 4.0) * 2;
      gy = (gy - canvas_h / 4.0) * 2;
    }
    double lift = list[i].lift * C.lift_scale * s2;   // canvas px
    double ab = smooth(0.15, 0.75, list[i].occb), as = smooth(0.15, 0.75, list[i].occs);
    if (ab <= 0.01 && as <= 0.01) continue;
    // ground point and ball centre on the target
    double Gx = cdx + gx * k, Gy = cdy + gy * k;
    double Bx = Gx, By = cdy + (gy - lift - C.rest_lift * rcanvas) * k;
    if (Gx < clip->x - 4 * rs || Gx > clip->x + clip->w + 4 * rs || Gy < clip->y - 4 * rs - lift * k ||
        Gy > clip->y + clip->h + 4 * rs)
      continue;
    // shadow: wider and fainter the higher the ball, shifted sideways by the sun
    if (sh && as > 0.01) {
      double spread = 1 + lift / (9 * rcanvas);
      if (spread > 3.2) spread = 3.2;
      double w = 2 * rs * C.shadow_size * (0.55 + 0.45 * spread), hh = w * C.shadow_flat;
      double alpha = C.shadow_alpha / (0.55 + 0.45 * spread) * as;
      SDL_SetTextureAlphaMod(sh, (Uint8)fmin(255, fmax(0, alpha * 255)));
      SDL_FRect d = {(float)(Gx + C.shadow_sun * lift * k - w / 2), (float)(Gy + 0.30 * rs - hh / 2), (float)w, (float)hh};
      SDL_RenderCopyF(rd, sh, NULL, &d);
    }
    if (bt && ab > 0.01) {
      SDL_SetTextureAlphaMod(bt, (Uint8)(ab * 255 + 0.5));
      SDL_FRect d = {(float)(Bx - D / 2.0), (float)(By - D / 2.0), (float)D, (float)D};
      SDL_RenderCopyF(rd, bt, NULL, &d);
    }
    if (list[i].speed > cap_speed) cap_speed = list[i].speed, cap_x = Bx, cap_y = By;
  }
  SDL_RenderSetClipRect(rd, NULL);
  if (g_cap.on) cap_frame(rd, cap_x, cap_y, cap_speed, now_ms);
}
