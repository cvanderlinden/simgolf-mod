// Frosted Mint UI kit: layout helpers, see ui_layout.h.
#include "port/ui/ui_layout.h"

#include <math.h>

float ui_scale_for_height(int screen_h) {
  if (screen_h <= 0) return 1.0f;
  float s = floorf(screen_h / 1080.0f * 4.0f + 0.5f) / 4.0f;
  return s < 0.75f ? 0.75f : s;
}

float ui_px(float scale, float v) { return floorf(v * scale + 0.5f); }

float ui_text_px(float scale, float size, float floor_px) {
  float v = size * scale;
  return v < floor_px ? floor_px : floorf(v + 0.5f);
}

UiRect ui_inset(UiRect r, float l, float t, float right, float b) {
  UiRect o = {r.x + l, r.y + t, r.w - l - right, r.h - t - b};
  if (o.w < 0) o.w = 0;
  if (o.h < 0) o.h = 0;
  return o;
}

UiRect ui_inset_all(UiRect r, float d) { return ui_inset(r, d, d, d, d); }
UiRect ui_translate(UiRect r, float dx, float dy) { return ui_rect(r.x + dx, r.y + dy, r.w, r.h); }

UiRect ui_anchor(UiRect p, float w, float h, UiAnchor a, float dx, float dy) {
  int col = a % 3, row = a / 3;
  float x = col == 0 ? p.x + dx : col == 1 ? p.x + (p.w - w) / 2 + dx : p.x + p.w - w - dx;
  float y = row == 0 ? p.y + dy : row == 1 ? p.y + (p.h - h) / 2 + dy : p.y + p.h - h - dy;
  return ui_rect(x, y, w, h);
}

UiRect ui_center(UiRect p, float w, float h) { return ui_anchor(p, w, h, UI_CENTER, 0, 0); }

int ui_rect_contains(UiRect r, float x, float y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

int ui_rrect_contains(UiRect r, float radius, float x, float y) {
  if (!ui_rect_contains(r, x, y)) return 0;
  float m = (r.w < r.h ? r.w : r.h) / 2;
  if (radius > m) radius = m;
  if (radius <= 0) return 1;
  // distance from the inner rectangle the corner circles are centred on
  float cx = x < r.x + radius ? r.x + radius : x > r.x + r.w - radius ? r.x + r.w - radius : x;
  float cy = y < r.y + radius ? r.y + radius : y > r.y + r.h - radius ? r.y + r.h - radius : y;
  float dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= radius * radius;
}

UiRect ui_rect_union(UiRect a, UiRect b) {
  if (a.w <= 0 || a.h <= 0) return b;
  if (b.w <= 0 || b.h <= 0) return a;
  float x0 = fminf(a.x, b.x), y0 = fminf(a.y, b.y);
  float x1 = fmaxf(a.x + a.w, b.x + b.w), y1 = fmaxf(a.y + a.h, b.y + b.h);
  return ui_rect(x0, y0, x1 - x0, y1 - y0);
}

UiRect ui_rect_intersect(UiRect a, UiRect b) {
  float x0 = fmaxf(a.x, b.x), y0 = fmaxf(a.y, b.y);
  float x1 = fminf(a.x + a.w, b.x + b.w), y1 = fminf(a.y + a.h, b.y + b.h);
  return ui_rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
}

UiRect ui_rect_round(UiRect r) {
  float x0 = floorf(r.x + 0.5f), y0 = floorf(r.y + 0.5f);
  float x1 = floorf(r.x + r.w + 0.5f), y1 = floorf(r.y + r.h + 0.5f);
  return ui_rect(x0, y0, x1 - x0, y1 - y0);
}

UiStack ui_stack(UiRect area, int horizontal, float gap) {
  UiStack s = {area, horizontal, gap, 0, 0};
  return s;
}

UiRect ui_stack_next(UiStack *s, float size) {
  float at = s->cursor + (s->count ? s->gap : 0);
  s->cursor = at + size;
  s->count++;
  return s->horizontal ? ui_rect(s->area.x + at, s->area.y, size, s->area.h)
                       : ui_rect(s->area.x, s->area.y + at, s->area.w, size);
}

UiRect ui_stack_next_sized(UiStack *s, float w, float h, float align) {
  UiRect slot = ui_stack_next(s, s->horizontal ? w : h);
  if (s->horizontal) slot.y += (slot.h - h) * align, slot.h = h;
  else slot.x += (slot.w - w) * align, slot.w = w;
  return slot;
}

float ui_stack_left(const UiStack *s) {
  float len = s->horizontal ? s->area.w : s->area.h;
  float used = s->cursor + (s->count ? s->gap : 0);
  return len - used > 0 ? len - used : 0;
}

float ui_row_length(int n, float size, float gap) { return n <= 0 ? 0 : n * size + (n - 1) * gap; }

int ui_utf8_cut(const char *s, int max) {
  int n = 0;
  while (n < max && s[n]) n++;
  if (!s[n]) return n;   // all of it fits
  while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;   // s[n] would start in the middle of a character: step back
  return n;
}
