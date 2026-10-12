// Frosted Mint UI kit (task-60): the golfer mood faces, see ui_faces.h.
#include "port/ui/ui_faces.h"

#include <math.h>
#include <string.h>

typedef struct {
  uint8_t top[3], bot[3];   // disc gradient, top to bottom
  float smile;              // mouth: offset of its middle from its ends, in mouth half widths (+ smile, - frown)
  float mouth_w;            // half width
  float mouth_y;
  int open;                 // 0 line, 1 open smile with teeth, 2 gritted teeth (angry)
  int eyes;                 // 0 dots, 1 happy arcs, 2 small dots
  float brow;               // + angry (inner ends low), - worried (inner ends high), 0 none
} Face;

// Index = golf's mood sprite, same meaning: 1 furious (gritted teeth), 2 sad, 3 unhappy, 4 meh,
// 5 flat, 6 slightly pleased, 7 pleased, 8 happy, 9 grinning, 10 delighted. 0 is only a neutral
// blank (golf's sprite 0 is not a face: it is the empty slot of the golfer list).
static const Face k_faces[UI_FACE_COUNT] = {
    /* 0 */ {{232, 240, 237}, {190, 207, 199}, 0.00f, 0.30f, 0.46f, 0, 2, 0.00f},
    /* 1 */ {{255, 138, 120}, {214, 58, 54}, -0.30f, 0.44f, 0.50f, 2, 0, 0.95f},
    /* 2 */ {{255, 160, 126}, {228, 84, 66}, -0.50f, 0.40f, 0.52f, 0, 0, -0.60f},
    /* 3 */ {{255, 178, 112}, {236, 120, 54}, -0.38f, 0.40f, 0.52f, 0, 0, 0.00f},
    /* 4 */ {{255, 204, 108}, {242, 160, 48}, -0.20f, 0.38f, 0.50f, 0, 0, 0.00f},
    /* 5 */ {{252, 228, 112}, {232, 192, 52}, 0.00f, 0.36f, 0.48f, 0, 0, 0.00f},
    /* 6 */ {{226, 234, 116}, {178, 206, 58}, 0.26f, 0.38f, 0.46f, 0, 0, 0.00f},
    /* 7 */ {{184, 234, 120}, {104, 204, 78}, 0.50f, 0.42f, 0.44f, 0, 0, 0.00f},
    /* 8 */ {{156, 236, 146}, {58, 194, 110}, 0.76f, 0.46f, 0.42f, 0, 0, 0.00f},
    /* 9 */ {{136, 240, 176}, {36, 188, 130}, 0.95f, 0.50f, 0.38f, 1, 0, 0.00f},
    /* 10 */ {{118, 242, 198}, {24, 180, 142}, 1.10f, 0.54f, 0.34f, 1, 1, 0.00f},
};

static const float INK[3] = {16, 52, 42};   // deep green, the kit's text colour family

typedef struct { float x[40], y[40]; int n; } Poly;

static float seg_dist(float px, float py, float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0, l = dx * dx + dy * dy;
  float t = l > 0 ? ((px - x0) * dx + (py - y0) * dy) / l : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  float ex = px - (x0 + t * dx), ey = py - (y0 + t * dy);
  return sqrtf(ex * ex + ey * ey);
}

static void quad_pts(Poly *p, float x0, float y0, float cx, float cy, float x1, float y1, int n) {
  for (int i = 0; i <= n && p->n < 40; i++) {
    float t = (float)i / n, u = 1 - t;
    p->x[p->n] = u * u * x0 + 2 * u * t * cx + t * t * x1;
    p->y[p->n] = u * u * y0 + 2 * u * t * cy + t * t * y1;
    p->n++;
  }
}

static float line_dist(const Poly *p, float x, float y) {
  float d = 1e9f;
  for (int i = 0; i + 1 < p->n; i++) {
    float s = seg_dist(x, y, p->x[i], p->y[i], p->x[i + 1], p->y[i + 1]);
    if (s < d) d = s;
  }
  return d;
}

// signed distance to a closed polygon (negative inside)
static float poly_sd(const Poly *p, float x, float y) {
  float d = 1e9f;
  int in = 0;
  for (int i = 0, j = p->n - 1; i < p->n; j = i++) {
    float s = seg_dist(x, y, p->x[j], p->y[j], p->x[i], p->y[i]);
    if (s < d) d = s;
    if (((p->y[i] > y) != (p->y[j] > y)) && (x < (p->x[j] - p->x[i]) * (y - p->y[i]) / (p->y[j] - p->y[i]) + p->x[i])) in = !in;
  }
  return in ? -d : d;
}

static float cov(float sd, float aa) {   // signed distance (unit disc coordinates) -> coverage
  float c = 0.5f - sd / aa;
  return c < 0 ? 0 : c > 1 ? 1 : c;
}

uint32_t ui_face_colour(int mood) {
  const Face *f = &k_faces[mood < 0 ? 0 : mood > 10 ? 10 : mood];
  return 0xFF000000u | (uint32_t)((f->top[0] + f->bot[0]) / 2) << 16 | (uint32_t)((f->top[1] + f->bot[1]) / 2) << 8 |
         (uint32_t)((f->top[2] + f->bot[2]) / 2);
}

int ui_face_raster(int mood, int size, uint32_t *argb) {
  if (!argb || size < 4 || size > 1024) return 0;
  const Face *f = &k_faces[mood < 0 ? 0 : mood > 10 ? 10 : mood];
  const float R = 0.94f;                 // the disc, in half sizes; the rest is room for the edge
  const float k = size / 2.0f;           // unit -> pixels
  const float aa = 1.0f / k;             // one pixel in unit coordinates
  const float sw = 0.075f;               // feature stroke half width
  // geometry in unit coordinates (y down)
  Poly mouth = {0}, arcL = {0}, arcR = {0};
  float mw = f->mouth_w, my = f->mouth_y - 0.12f * (f->smile > 0 ? f->smile * 0.4f : 0);
  quad_pts(&mouth, -mw, my, 0, my + 2 * f->smile * mw * 0.55f, mw, my, 20);
  float ex = 0.34f, ey = -0.18f;
  if (f->eyes == 1) {
    quad_pts(&arcL, -ex - 0.15f, ey + 0.07f, -ex, ey - 0.20f, -ex + 0.15f, ey + 0.07f, 10);
    quad_pts(&arcR, ex - 0.15f, ey + 0.07f, ex, ey - 0.20f, ex + 0.15f, ey + 0.07f, 10);
  }
  Poly browL = {0}, browR = {0};
  if (f->brow != 0) {
    float t = f->brow * 0.16f, by = -0.50f;
    // inner end (towards the nose) lower for anger, higher for worry
    browL.x[0] = -ex - 0.19f, browL.y[0] = by - t;
    browL.x[1] = -ex + 0.17f, browL.y[1] = by + t;
    browL.n = 2;
    browR.x[0] = ex + 0.19f, browR.y[0] = by - t;
    browR.x[1] = ex - 0.17f, browR.y[1] = by + t;
    browR.n = 2;
  }
  for (int py = 0; py < size; py++)
    for (int px = 0; px < size; px++) {
      float x = ((px + 0.5f) - k) / k, y = ((py + 0.5f) - k) / k;
      float r = sqrtf(x * x + y * y);
      float disc = cov(r - R, aa);
      if (disc <= 0) {
        argb[py * size + px] = 0;
        continue;
      }
      float g = (y / R + 1) * 0.5f;
      g = g < 0 ? 0 : g > 1 ? 1 : g;
      float c[3];
      for (int i = 0; i < 3; i++) c[i] = f->top[i] + (f->bot[i] - f->top[i]) * (0.3f + 0.7f * g);
      // a deeper tint towards the rim, and a pale line right at it (glass edge)
      float rim = cov(-(r - (R - 0.10f)), aa * 3);   // 1 at the very edge zone
      rim = 1 - rim;
      for (int i = 0; i < 3; i++) c[i] = c[i] * (1 - 0.18f * rim) ;
      float edge = cov(-(r - (R - 0.045f)), aa);
      edge = 1 - edge;   // 1 in the outer 0.045
      for (int i = 0; i < 3; i++) c[i] += (255 - c[i]) * 0.26f * edge;
      // top gloss: a soft ellipse in the upper third
      float gx = x / 0.66f, gy = (y + 0.56f) / 0.30f;
      float gl = 1 - sqrtf(gx * gx + gy * gy);
      gl = gl < 0 ? 0 : gl > 1 ? 1 : gl;
      gl = gl * 0.16f;
      for (int i = 0; i < 3; i++) c[i] += (255 - c[i]) * gl;
      // features
      float ink = 0;
      float d;
      if (f->eyes == 1) {
        d = fminf(line_dist(&arcL, x, y), line_dist(&arcR, x, y)) - sw * 1.15f;
        ink = fmaxf(ink, cov(d, aa));
      } else {
        float er = f->eyes == 2 ? 0.075f : 0.115f;
        d = fminf(hypotf(x + ex, y - ey), hypotf(x - ex, y - ey)) - er;
        ink = fmaxf(ink, cov(d, aa));
      }
      if (f->brow != 0) {
        d = fminf(line_dist(&browL, x, y), line_dist(&browR, x, y)) - sw;
        ink = fmaxf(ink, cov(d, aa));
      }
      if (f->open) {
        // the open mouth: a lens between the mouth's flat edge and a curve, teeth along the flat edge
        float t = fabsf(x) / mw;
        float shape = 1 - t * t;
        float dd, band;
        if (f->open == 1) {
          float depth = (f->smile - 0.6f) * 0.10f + 0.08f;
          float h = (f->smile * mw * 0.55f + depth * 2) * shape;
          dd = fminf(y - my, (my + h) - y);
          band = (y - my) / (h > 0.001f ? h : 0.001f);   // 0 at the flat top edge, 1 at the bottom
        } else {
          float h = (fabsf(f->smile) * mw * 0.55f + 0.16f) * shape;
          dd = fminf(my - y, y - (my - h));
          band = (my - y) / (h > 0.001f ? h : 0.001f);
        }
        dd = fminf(dd, (mw - fabsf(x)) * 1.4f);
        float in = cov(-dd, aa);
        float teeth = f->open == 2 ? 1.0f : cov((band - 0.38f) * 0.25f, aa * 0.5f);
        // gritted: one dark line through the teeth
        if (f->open == 2 && fabsf(band - 0.5f) < 0.06f) teeth = 0.0f;
        ink = fmaxf(ink, in);
        for (int i = 0; i < 3; i++) c[i] += (INK[i] - c[i]) * ink;
        ink = 0;
        float tw = in * teeth * (dd > aa * 2 ? 1.0f : cov(-(dd - aa * 2), aa));
        for (int i = 0; i < 3; i++) c[i] += (250 - c[i]) * tw;
      } else {
        d = line_dist(&mouth, x, y) - sw;
        ink = fmaxf(ink, cov(d, aa));
      }
      for (int i = 0; i < 3; i++) c[i] += (INK[i] - c[i]) * ink;
      if (mood == 2) {   // a tear under the left eye
        float td = hypotf(x + 0.40f, y - 0.10f) - 0.075f, tdd = hypotf(x + 0.40f, y - 0.06f) - 0.05f;
        float tc = fmaxf(cov(td, aa), cov(tdd, aa));
        static const float TEAR[3] = {170, 224, 255};
        for (int i = 0; i < 3; i++) c[i] += (TEAR[i] - c[i]) * tc * 0.95f;
      }
      int ri = (int)(c[0] + 0.5f), gi = (int)(c[1] + 0.5f), bi = (int)(c[2] + 0.5f);
      ri = ri > 255 ? 255 : ri, gi = gi > 255 ? 255 : gi, bi = bi > 255 ? 255 : bi;
      int a = (int)(disc * 255 + 0.5f);
      argb[py * size + px] = (uint32_t)a << 24 | (uint32_t)ri << 16 | (uint32_t)gi << 8 | (uint32_t)bi;
    }
  return 1;
}
