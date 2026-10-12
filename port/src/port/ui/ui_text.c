// Frosted Mint UI kit: glyph atlas text, see ui_text.h.
#include "port/ui/ui_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// gdi.c has its own static copy; the kit is also built without the game.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

static const char *const k_cuts[UIT_FILES] = {"Regular", "Medium", "SemiBold", "Bold", "ExtraBold"};
static const int k_weights[UIT_FILES] = {400, 500, 600, 700, 800};

#define INFO(t, i) ((stbtt_fontinfo *)(t)->f[i].info)

static unsigned char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *d = n > 0 ? malloc((size_t)n) : NULL;
  if (d && fread(d, 1, (size_t)n, f) != (size_t)n) {
    free(d);
    d = NULL;
  }
  fclose(f);
  return d;
}

static void atlas_reset(UiText *t) {
  memset(t->alpha, 0, (size_t)t->aw * t->ah);
  memset(t->cache, 0, sizeof t->cache);
  t->nglyphs = 0;
  t->shelf_x = t->shelf_y = 1;
  t->shelf_h = 0;
  t->dirty = 1;
  t->dx0 = t->dy0 = 0;
  t->dx1 = t->aw;
  t->dy1 = t->ah;
}

int uit_init(UiText *t, SDL_Renderer *r, const char *font_dir, float gamma) {
  memset(t, 0, sizeof *t);
  t->r = r;
  int loaded = 0;
  for (int i = 0; i < UIT_FILES; i++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/PlusJakartaSans-%s.ttf", font_dir, k_cuts[i]);
    t->f[i].weight = k_weights[i];
    t->f[i].info = calloc(1, sizeof(stbtt_fontinfo));
    t->f[i].data = read_file(path);
    if (!t->f[i].data || !stbtt_InitFont(INFO(t, i), t->f[i].data, stbtt_GetFontOffsetForIndex(t->f[i].data, 0))) {
      fprintf(stderr, "ui: font %s not loaded\n", path);
      continue;
    }
    t->f[i].ok = 1;
    loaded++;
    // Tabular figures follow the figures by ten glyph ids in every cut (gdi.c checks the same).
    int ok = 1, adv0 = -1, lsb;
    for (int k = 0; k < 10 && ok; k++) {
      int g = stbtt_FindGlyphIndex(INFO(t, i), '0' + k), a, pa;
      if (!g) {
        ok = 0;
        break;
      }
      stbtt_GetGlyphHMetrics(INFO(t, i), g + 10, &a, &lsb);
      stbtt_GetGlyphHMetrics(INFO(t, i), g, &pa, &lsb);
      if (adv0 < 0) adv0 = a;
      if (a != adv0 || (k == 1 && a == pa)) ok = 0;
      t->f[i].tn[k] = g + 10;
    }
    if (!ok) memset(t->f[i].tn, 0, sizeof t->f[i].tn);
  }
  t->gamma = gamma > 0 ? gamma : 1;
  for (int i = 0; i < 256; i++) t->curve[i] = (uint8_t)(powf(i / 255.0f, 1.0f / t->gamma) * 255.0f + 0.5f);
  t->aw = t->ah = 2048;
  t->alpha = malloc((size_t)t->aw * t->ah);
  if (r) {
    t->atlas = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, t->aw, t->ah);
    if (t->atlas) {
      SDL_SetTextureBlendMode(t->atlas, SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(t->atlas, SDL_ScaleModeNearest);
    }
  }
  atlas_reset(t);
  return loaded;
}

void uit_free(UiText *t) {
  for (int i = 0; i < UIT_FILES; i++) {
    free(t->f[i].data);
    free(t->f[i].info);
  }
  if (t->atlas) SDL_DestroyTexture(t->atlas);
  free(t->alpha);
  free(t->v);
  free(t->idx);
  memset(t, 0, sizeof *t);
}

static int pick(UiText *t, int weight) {
  int best = -1, bd = 1 << 30;
  for (int i = 0; i < UIT_FILES; i++) {
    if (!t->f[i].ok) continue;
    int dd = abs(t->f[i].weight - weight);
    if (dd < bd) bd = dd, best = i;
  }
  return best;
}

// Next code point; invalid UTF-8 bytes are Windows-1252.
static uint32_t next_cp(const char **ps) {
  static const uint16_t cp1252[32] = {0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                                      0x2039, 0x0152, 0, 0x017D, 0, 0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
                                      0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};
  const unsigned char *s = (const unsigned char *)*ps;
  uint32_t c = s[0];
  int n = c >= 0xF0 && c < 0xF8 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 && c < 0xE0 ? 1 : 0;
  if (c >= 0x80 && n) {
    uint32_t v = c & (0x3F >> n);
    int ok = 1;
    for (int i = 1; i <= n; i++) {
      if ((s[i] & 0xC0) != 0x80) {
        ok = 0;
        break;
      }
      v = v << 6 | (s[i] & 0x3F);
    }
    if (ok) {
      *ps += n + 1;
      return v;
    }
  }
  *ps += 1;
  if (c >= 0x80 && c < 0xA0) return cp1252[c - 0x80] ? cp1252[c - 0x80] : '?';
  return c;
}

static int glyph_index(UiText *t, int file, uint32_t cp) {
  if (cp >= '0' && cp <= '9' && t->f[file].tn[0]) return t->f[file].tn[cp - '0'];
  int g = stbtt_FindGlyphIndex(INFO(t, file), (int)cp);
  if (!g && cp == 0x2009) g = stbtt_FindGlyphIndex(INFO(t, file), ' ');   // thin space
  return g;
}

static UitGlyph *glyph(UiText *t, int file, int px, int g) {
  uint32_t key = 0x80000000u | (uint32_t)file << 26 | (uint32_t)(px & 1023) << 16 | (uint32_t)(g & 0xFFFF);
  uint32_t h = (key * 2654435761u) >> 19;   // 13 bits
  for (int probe = 0; probe < UIT_CACHE; probe++) {
    UitGlyph *e = &t->cache[(h + probe) & (UIT_CACHE - 1)];
    if (e->key == key) return e;
    if (e->key) continue;
    if (t->nglyphs > UIT_CACHE * 3 / 4) {
      atlas_reset(t);
      t->resets++;
      return glyph(t, file, px, g);
    }
    stbtt_fontinfo *info = INFO(t, file);
    float scale = stbtt_ScaleForPixelHeight(info, (float)px);
    // ScaleForPixelHeight is ascent-descent; the design sizes are em sizes (CSS font-size).
    scale = stbtt_ScaleForMappingEmToPixels(info, (float)px);
    int adv, lsb, x0, y0, x1, y1;
    stbtt_GetGlyphHMetrics(info, g, &adv, &lsb);
    stbtt_GetGlyphBitmapBox(info, g, scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0, hh = y1 - y0;
    if (w > 0 && hh > 0) {
      if (t->shelf_x + w + 1 > t->aw) {
        t->shelf_x = 1;
        t->shelf_y += t->shelf_h + 1;
        t->shelf_h = 0;
      }
      if (t->shelf_y + hh + 1 > t->ah) {
        atlas_reset(t);
        t->resets++;
        return glyph(t, file, px, g);
      }
      uint8_t *dst = t->alpha + (size_t)t->shelf_y * t->aw + t->shelf_x;
      stbtt_MakeGlyphBitmap(info, dst, w, hh, t->aw, scale, scale, g);
      if (t->gamma != 1)
        for (int y = 0; y < hh; y++)
          for (int x = 0; x < w; x++) dst[(size_t)y * t->aw + x] = t->curve[dst[(size_t)y * t->aw + x]];
      e->x = (int16_t)t->shelf_x;
      e->y = (int16_t)t->shelf_y;
      if (!t->dirty) {
        t->dirty = 1;
        t->dx0 = t->shelf_x, t->dy0 = t->shelf_y, t->dx1 = t->shelf_x + w, t->dy1 = t->shelf_y + hh;
      } else {
        if (t->shelf_x < t->dx0) t->dx0 = t->shelf_x;
        if (t->shelf_y < t->dy0) t->dy0 = t->shelf_y;
        if (t->shelf_x + w > t->dx1) t->dx1 = t->shelf_x + w;
        if (t->shelf_y + hh > t->dy1) t->dy1 = t->shelf_y + hh;
      }
      t->shelf_x += w + 1;
      if (hh > t->shelf_h) t->shelf_h = hh;
    }
    e->key = key;
    e->w = (int16_t)(w > 0 ? w : 0);
    e->h = (int16_t)(hh > 0 ? hh : 0);
    e->xoff = (int16_t)x0;
    e->yoff = (int16_t)y0;
    e->adv = adv * scale;
    t->nglyphs++;
    return e;
  }
  return NULL;
}

static void upload(UiText *t) {
  if (!t->dirty || !t->atlas) return;
  int w = t->dx1 - t->dx0, h = t->dy1 - t->dy0;
  if (w > 0 && h > 0) {
    uint32_t *px = malloc((size_t)w * h * 4);
    if (px) {
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
          px[(size_t)y * w + x] = (uint32_t)t->alpha[(size_t)(t->dy0 + y) * t->aw + t->dx0 + x] << 24 | 0xFFFFFFu;
      SDL_Rect r = {t->dx0, t->dy0, w, h};
      SDL_UpdateTexture(t->atlas, &r, px, w * 4);
      free(px);
    }
  }
  t->dirty = 0;
}

float uit_measure(UiText *t, int weight, float px, const char *s, float tracking) {
  int f = pick(t, weight);
  if (f < 0 || !s) return 0;
  int ipx = (int)(px + 0.5f);
  float x = 0;
  int prev = 0;
  stbtt_fontinfo *info = INFO(t, f);
  float scale = stbtt_ScaleForMappingEmToPixels(info, (float)ipx);
  while (*s) {
    uint32_t cp = next_cp(&s);
    int g = glyph_index(t, f, cp);
    int adv, lsb;
    stbtt_GetGlyphHMetrics(info, g, &adv, &lsb);
    if (prev) x += stbtt_GetGlyphKernAdvance(info, prev, g) * scale;
    x += adv * scale + tracking * ipx;
    prev = g;
  }
  return x;
}

void uit_metrics(UiText *t, int weight, float px, float *ascent, float *descent) {
  int f = pick(t, weight);
  *ascent = px * 0.8f;
  *descent = px * 0.2f;
  if (f < 0) return;
  int a, d, gap;
  stbtt_GetFontVMetrics(INFO(t, f), &a, &d, &gap);
  float scale = stbtt_ScaleForMappingEmToPixels(INFO(t, f), (float)(int)(px + 0.5f));
  *ascent = a * scale;
  *descent = -d * scale;
}

float uit_draw(UiText *t, float x, float y, int weight, float px, const char *s, float tracking, UiColor c) {
  int f = pick(t, weight);
  if (f < 0 || !s || !*s || !t->atlas) return x;
  int ipx = (int)(px + 0.5f);
  stbtt_fontinfo *info = INFO(t, f);
  float scale = stbtt_ScaleForMappingEmToPixels(info, (float)ipx);
  size_t n = strlen(s);
  if ((int)n * 4 > t->cap) {
    t->cap = (int)n * 4 + 64;
    t->v = realloc(t->v, sizeof *t->v * t->cap);
    t->idx = realloc(t->idx, sizeof *t->idx * t->cap / 4 * 6);
  }
  t->nv = t->ni = 0;
  float by = floorf(y + 0.5f), pen = x;
  int prev = 0;
  float iw = 1.0f / t->aw, ih = 1.0f / t->ah;
  SDL_Color col = {c.r, c.g, c.b, c.a};
  while (*s) {
    uint32_t cp = next_cp(&s);
    int g = glyph_index(t, f, cp);
    if (prev) pen += stbtt_GetGlyphKernAdvance(info, prev, g) * scale;
    UitGlyph *e = glyph(t, f, ipx, g);
    if (!e) break;
    if (e->w > 0) {
      float gx = floorf(pen + 0.5f) + e->xoff, gy = by + e->yoff;
      float u0 = e->x * iw, v0 = e->y * ih, u1 = (e->x + e->w) * iw, v1 = (e->y + e->h) * ih;
      SDL_Vertex *v = &t->v[t->nv];
      v[0] = (SDL_Vertex){{gx, gy}, col, {u0, v0}};
      v[1] = (SDL_Vertex){{gx + e->w, gy}, col, {u1, v0}};
      v[2] = (SDL_Vertex){{gx + e->w, gy + e->h}, col, {u1, v1}};
      v[3] = (SDL_Vertex){{gx, gy + e->h}, col, {u0, v1}};
      int *ix = &t->idx[t->ni];
      ix[0] = t->nv, ix[1] = t->nv + 1, ix[2] = t->nv + 2;
      ix[3] = t->nv, ix[4] = t->nv + 2, ix[5] = t->nv + 3;
      t->nv += 4;
      t->ni += 6;
    }
    pen += e->adv + tracking * ipx;
    prev = g;
  }
  upload(t);
  if (t->ni) SDL_RenderGeometry(t->r, t->atlas, t->v, t->nv, t->idx, t->ni);
  return pen;
}

int uit_fit(UiText *t, int weight, float px, const char *s, float tracking, float max_w) {
  int n = (int)strlen(s), best = 0, last_space = -1;
  char buf[1024];
  for (int i = 1; i <= n && i < (int)sizeof buf; i++) {
    memcpy(buf, s, (size_t)i);
    buf[i] = 0;
    if (uit_measure(t, weight, px, buf, tracking) > max_w) break;
    best = i;
    if (s[i] == ' ' || s[i] == 0) last_space = i;
  }
  if (best < n && last_space > 0) return last_space;
  return best;
}
