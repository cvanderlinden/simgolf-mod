// Frosted Mint UI kit: design tokens, see ui_tokens.h.
#include "port/ui/ui_tokens.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tomlc99/toml.h"

enum { T_COLOR, T_NUM, T_GRAD, T_SHADOW, T_TYPE, T_SPACE };
typedef struct { const char *key; int kind; size_t off; } Entry;

#define E(k, kind, field) {k, kind, offsetof(UiTokens, field)}
static const Entry k_entries[] = {
    E("glass.fill", T_COLOR, glass_fill),
    E("glass.fill_strong", T_COLOR, glass_fill_strong),
    E("glass.border", T_COLOR, glass_border),
    E("glass.highlight", T_COLOR, glass_highlight),
    E("glass.solid", T_COLOR, glass_solid),
    E("glass.shadow", T_SHADOW, glass_shadow),
    E("glass.blur", T_NUM, glass_blur),
    E("glass.saturation", T_NUM, glass_saturation),
    E("surface.inset", T_COLOR, surface_inset),
    E("surface.inset_border", T_COLOR, surface_inset_border),
    E("text.primary", T_COLOR, text_primary),
    E("text.secondary", T_COLOR, text_secondary),
    E("text.tertiary", T_COLOR, text_tertiary),
    E("text.on_accent", T_COLOR, text_on_accent),
    E("accent.color", T_COLOR, accent),
    E("accent.soft", T_COLOR, accent_soft),
    E("accent.soft_text", T_COLOR, accent_soft_text),
    E("accent.grad", T_GRAD, accent_grad),
    E("money", T_COLOR, money),
    E("danger", T_COLOR, danger),
    E("warn.grad", T_GRAD, warn_grad),
    E("meter.grad", T_GRAD, meter_grad),
    E("meter.track", T_COLOR, meter_track),
    E("divider", T_COLOR, divider),
    E("seg.track", T_COLOR, seg_track),
    E("seg.on", T_COLOR, seg_on),
    E("seg.on_shadow", T_SHADOW, seg_on_shadow),
    E("tile.fill", T_COLOR, tile_fill),
    E("tile.fill_on", T_COLOR, tile_fill_on),
    E("dock.square", T_COLOR, dock_fill),
    E("dock.square_shadow", T_SHADOW, dock_square_shadow),
    E("dock.active_shadow", T_SHADOW, dock_active_shadow),
    E("icon.muted", T_COLOR, icon_muted),
    E("slider.thumb_shadow", T_SHADOW, thumb_shadow),
    E("world.bg_top", T_COLOR, world_bg_top),
    E("world.bg_mid", T_COLOR, world_bg_mid),
    E("world.bg_bottom", T_COLOR, world_bg_bottom),
    E("scrim", T_COLOR, scrim),
    E("radius.panel", T_NUM, radius_panel),
    E("radius.card", T_NUM, radius_card),
    E("radius.tile", T_NUM, radius_tile),
    E("radius.menu_tile", T_NUM, radius_menu_tile),
    E("radius.control", T_NUM, radius_control),
    E("radius.seg_inner", T_NUM, radius_seg_inner),
    E("radius.tray", T_NUM, radius_tray),
    E("space", T_SPACE, space),
    E("pad.panel", T_NUM, pad_panel),
    E("pad.tray", T_NUM, pad_tray),
    E("pad.card", T_NUM, pad_card),
    E("pad.dialog", T_NUM, pad_dialog),
    E("gap.tiles", T_NUM, gap_tiles),
    E("edge.margin", T_NUM, edge_margin),
    E("size.capsule_h", T_NUM, capsule_h),
    E("size.capsule_pad", T_NUM, capsule_pad),
    E("size.emblem", T_NUM, emblem),
    E("size.dock_item_w", T_NUM, dock_item_w),
    E("size.dock_square", T_NUM, dock_square),
    E("size.dock_square_on", T_NUM, dock_square_on),
    E("size.dock_icon", T_NUM, dock_icon),
    E("size.dock_pad_x", T_NUM, dock_pad_x),
    E("size.dock_pad_top", T_NUM, dock_pad_top),
    E("size.dock_pad_bottom", T_NUM, dock_pad_bottom),
    E("size.dock_bottom", T_NUM, dock_bottom),
    E("size.tray_bottom", T_NUM, tray_bottom),
    E("size.tile_w", T_NUM, tile_w),
    E("size.tile_art_w", T_NUM, tile_art_w),
    E("size.tile_art_h", T_NUM, tile_art_h),
    E("size.icon_button", T_NUM, icon_button),
    E("size.icon_button_sm", T_NUM, icon_button_sm),
    E("size.button_h", T_NUM, button_h),
    E("size.pill_h", T_NUM, pill_h),
    E("size.seg_h", T_NUM, seg_h),
    E("size.tooltip_h", T_NUM, tooltip_h),
    E("size.tooltip_delay_ms", T_NUM, tooltip_delay_ms),
    E("size.icon_stroke", T_NUM, icon_stroke),
    E("size.meter_h", T_NUM, meter_h),
    E("size.slider_track_h", T_NUM, slider_track_h),
    E("size.slider_thumb", T_NUM, slider_thumb),
    E("size.toggle_w", T_NUM, toggle_w),
    E("size.toggle_h", T_NUM, toggle_h),
    E("size.dialog_min_w", T_NUM, dialog_min_w),
    E("size.dialog_max_w", T_NUM, dialog_max_w),
    E("size.popover_w", T_NUM, popover_w),
    E("size.popover_arrow", T_NUM, popover_arrow),
    E("size.bubble_max_w", T_NUM, bubble_max_w),
    E("size.cam_button", T_NUM, cam_button),
    E("type.display", T_TYPE, type[UI_TYPE_DISPLAY]),
    E("type.title", T_TYPE, type[UI_TYPE_TITLE]),
    E("type.heading", T_TYPE, type[UI_TYPE_HEADING]),
    E("type.number", T_TYPE, type[UI_TYPE_NUMBER]),
    E("type.body", T_TYPE, type[UI_TYPE_BODY]),
    E("type.label", T_TYPE, type[UI_TYPE_LABEL]),
    E("type.caption", T_TYPE, type[UI_TYPE_CAPTION]),
    E("type.micro", T_TYPE, type[UI_TYPE_MICRO]),
    E("type.quote", T_TYPE, type[UI_TYPE_QUOTE]),
    E("type.floor", T_NUM, text_floor),
    E("type.gamma", T_NUM, text_gamma),
    E("motion.hover", T_NUM, motion_hover),
    E("motion.press", T_NUM, motion_press),
    E("motion.open", T_NUM, motion_open),
    E("motion.close", T_NUM, motion_close),
};
#define N_ENTRIES ((int)(sizeof k_entries / sizeof k_entries[0]))

static UiColor rgba(int r, int g, int b, float a) {
  UiColor c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)(a * 255.0f + 0.5f)};
  return c;
}
static UiColor hex(uint32_t v) { return rgba((v >> 16) & 255, (v >> 8) & 255, v & 255, 1.0f); }
static UiType ty(float size, float line, int weight, float tracking) {
  UiType t = {size, line, tracking, weight};
  return t;
}

void ui_tokens_default(UiTokens *t) {
  memset(t, 0, sizeof *t);
  t->glass_fill = rgba(214, 255, 232, 0.66f);
  t->glass_fill_strong = rgba(236, 255, 245, 0.72f);
  t->glass_border = rgba(230, 255, 240, 0.75f);
  t->glass_highlight = rgba(255, 255, 255, 0.90f);
  t->glass_solid = rgba(226, 247, 236, 0.94f);
  t->glass_shadow = (UiShadow){0, 18, 50, rgba(0, 60, 30, 0.25f)};
  t->glass_blur = 30;
  t->glass_saturation = 2.0f;
  t->surface_inset = rgba(236, 255, 245, 0.50f);
  t->surface_inset_border = rgba(255, 255, 255, 0.80f);
  t->text_primary = hex(0x0B2A1C);
  t->text_secondary = hex(0x283E34);
  t->text_tertiary = rgba(0x4A, 0x5F, 0x54, 0.80f);
  t->text_on_accent = hex(0xFFFFFF);
  t->accent = hex(0x0E9F6E);
  t->accent_soft = rgba(14, 159, 110, 0.15f);
  t->accent_soft_text = hex(0x0B7A55);
  t->accent_grad = (UiGrad){hex(0x22C38E), hex(0x0B7A55), 160};
  t->money = hex(0xD9961A);
  t->danger = hex(0xD9583B);
  t->warn_grad = (UiGrad){hex(0xF08A5D), hex(0xF5B841), 90};
  t->meter_grad = (UiGrad){hex(0x4FD1A5), hex(0x0E9F6E), 90};
  t->meter_track = rgba(255, 255, 255, 0.60f);
  t->divider = rgba(16, 35, 26, 0.12f);
  t->seg_track = rgba(255, 255, 255, 0.45f);
  t->seg_on = hex(0xFFFFFF);
  t->seg_on_shadow = (UiShadow){0, 1, 4, rgba(0, 0, 0, 0.12f)};
  t->tile_fill = rgba(255, 255, 255, 0.55f);
  t->tile_fill_on = rgba(236, 255, 245, 0.85f);
  t->dock_fill = rgba(236, 255, 245, 0.60f);
  t->dock_square_shadow = (UiShadow){0, 4, 12, rgba(0, 0, 0, 0.10f)};
  t->dock_active_shadow = (UiShadow){0, 10, 24, rgba(14, 143, 100, 0.45f)};
  t->icon_muted = hex(0x2D4A3C);
  t->thumb_shadow = (UiShadow){0, 2, 6, rgba(0, 40, 20, 0.25f)};
  t->world_bg_top = hex(0xBFEFD9);
  t->world_bg_mid = hex(0xE2F7EC);
  t->world_bg_bottom = hex(0xBFE3CC);
  t->scrim = rgba(0, 20, 10, 0.10f);
  t->radius_panel = 30;
  t->radius_card = 26;
  t->radius_tile = 18;
  t->radius_menu_tile = 22;
  t->radius_control = 12;
  t->radius_seg_inner = 9;
  t->radius_tray = 28;
  static const float sp[8] = {4, 8, 12, 16, 20, 24, 32, 40};
  memcpy(t->space, sp, sizeof sp);
  t->pad_panel = 20;
  t->pad_tray = 16;
  t->pad_card = 20;
  t->pad_dialog = 24;
  t->gap_tiles = 8;
  t->edge_margin = 22;
  t->capsule_h = 62;
  t->capsule_pad = 8;
  t->emblem = 46;
  t->dock_item_w = 84;
  t->dock_square = 62;
  t->dock_square_on = 74;
  t->dock_icon = 28;
  t->dock_pad_x = 14;
  t->dock_pad_top = 10;
  t->dock_pad_bottom = 8;
  t->dock_bottom = 20;
  t->tray_bottom = 128;
  t->tile_w = 108;
  t->tile_art_w = 88;
  t->tile_art_h = 62;
  t->icon_button = 40;
  t->icon_button_sm = 32;
  t->button_h = 44;
  t->pill_h = 40;
  t->seg_h = 32;
  t->tooltip_h = 30;
  t->tooltip_delay_ms = 300;
  t->icon_stroke = 1.8f;
  t->meter_h = 8;
  t->slider_track_h = 6;
  t->slider_thumb = 20;
  t->toggle_w = 44;
  t->toggle_h = 26;
  t->dialog_min_w = 520;
  t->dialog_max_w = 720;
  t->popover_w = 400;
  t->popover_arrow = 24;
  t->bubble_max_w = 360;
  t->cam_button = 44;
  t->type[UI_TYPE_DISPLAY] = ty(34, 40, 800, 0);
  t->type[UI_TYPE_TITLE] = ty(24, 30, 800, 0);
  t->type[UI_TYPE_HEADING] = ty(20, 26, 800, 0);
  t->type[UI_TYPE_NUMBER] = ty(17, 22, 800, 0);
  t->type[UI_TYPE_BODY] = ty(15, 21, 600, 0);
  t->type[UI_TYPE_LABEL] = ty(13, 18, 700, 0);
  t->type[UI_TYPE_CAPTION] = ty(12, 16, 600, 0);
  t->type[UI_TYPE_MICRO] = ty(11, 14, 700, 0.02f);
  t->type[UI_TYPE_QUOTE] = ty(17, 24, 700, 0);
  t->text_floor = 11;
  t->text_gamma = 1.0f;
  t->motion_hover = 120;
  t->motion_press = 80;
  t->motion_open = 220;
  t->motion_close = 160;
}

const char *ui_tokens_key(int i) { return i >= 0 && i < N_ENTRIES ? k_entries[i].key : NULL; }

// ------------------------------------------------------------------ parsing

static void report(char *err, int errsz, const char *fmt, const char *a, const char *b) {
  if (!err || errsz <= 0) return;
  size_t n = strlen(err);
  if ((int)n >= errsz - 1) return;
  snprintf(err + n, (size_t)errsz - n, fmt, a, b);
}

int ui_color_parse(const char *s, UiColor *out) {
  while (*s == ' ') s++;
  if (*s == '#') {
    size_t n = strlen(s + 1);
    unsigned v = 0;
    for (size_t i = 0; i < n; i++)
      if (!isxdigit((unsigned char)s[1 + i])) return 0;
    if (n == 6 && sscanf(s + 1, "%6x", &v) == 1) {
      *out = (UiColor){(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, 255};
      return 1;
    }
    if (n == 8 && sscanf(s + 1, "%8x", &v) == 1) {
      *out = (UiColor){(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
      return 1;
    }
    return 0;
  }
  float r, g, b, a = 1;
  char tail = 0;
  if (sscanf(s, "rgba(%f,%f,%f,%f%c", &r, &g, &b, &a, &tail) == 5 && tail == ')') {
  } else if (sscanf(s, "rgb(%f,%f,%f%c", &r, &g, &b, &tail) == 4 && tail == ')') {
    a = 1;
  } else {
    return 0;
  }
  if (r < 0 || g < 0 || b < 0 || r > 255 || g > 255 || b > 255 || a < 0 || a > 1) return 0;
  *out = (UiColor){(uint8_t)(r + 0.5f), (uint8_t)(g + 0.5f), (uint8_t)(b + 0.5f), (uint8_t)(a * 255.0f + 0.5f)};
  return 1;
}

static int raw_num(toml_raw_t raw, float *out) {
  int64_t i;
  double d;
  if (!raw) return 0;
  if (!toml_rtoi(raw, &i)) {
    *out = (float)i;
    return 1;
  }
  if (!toml_rtod(raw, &d)) {
    *out = (float)d;
    return 1;
  }
  return 0;
}

static int raw_color(toml_raw_t raw, UiColor *out) {
  char *s = NULL;
  if (!raw || toml_rtos(raw, &s)) return 0;
  int ok = ui_color_parse(s, out);
  free(s);
  return ok;
}

static int num_in(const toml_table_t *t, const char *k, float *out) { return raw_num(toml_raw_in(t, k), out); }

static int apply(UiTokens *tok, const Entry *e, const toml_table_t *parent, const char *k, char *err, int errsz) {
  char *base = (char *)tok + e->off;
  toml_raw_t raw = toml_raw_in(parent, k);
  switch (e->kind) {
    case T_COLOR:
      if (raw_color(raw, (UiColor *)base)) return 1;
      break;
    case T_NUM:
      if (raw_num(raw, (float *)base)) return 1;
      break;
    case T_SPACE: {
      toml_array_t *a = toml_array_in(parent, k);
      if (!a) break;
      float v[8];
      int n = toml_array_nelem(a);
      if (n != 8) break;
      for (int i = 0; i < 8; i++)
        if (!raw_num(toml_raw_at(a, i), &v[i])) return report(err, errsz, "%s: space needs 8 numbers%s\n", e->key, ""), 0;
      memcpy(base, v, sizeof v);
      return 1;
    }
    case T_GRAD: {
      toml_table_t *t = toml_table_in(parent, k);
      UiGrad g = *(UiGrad *)base;
      if (!t || !raw_color(toml_raw_in(t, "from"), &g.from) || !raw_color(toml_raw_in(t, "to"), &g.to)) break;
      num_in(t, "angle", &g.angle);
      *(UiGrad *)base = g;
      return 1;
    }
    case T_SHADOW: {
      toml_table_t *t = toml_table_in(parent, k);
      UiShadow s = *(UiShadow *)base;
      if (!t) break;
      num_in(t, "x", &s.x);
      num_in(t, "y", &s.y);
      num_in(t, "blur", &s.blur);
      if (toml_raw_in(t, "color") && !raw_color(toml_raw_in(t, "color"), &s.color)) break;
      *(UiShadow *)base = s;
      return 1;
    }
    case T_TYPE: {
      toml_table_t *t = toml_table_in(parent, k);
      UiType y = *(UiType *)base;
      float w = (float)y.weight;
      if (!t) break;
      num_in(t, "size", &y.size);
      num_in(t, "line", &y.line);
      num_in(t, "tracking", &y.tracking);
      if (num_in(t, "weight", &w)) y.weight = (int)w;
      if (y.size <= 0 || y.line <= 0 || y.weight < 100 || y.weight > 900) break;
      *(UiType *)base = y;
      return 1;
    }
  }
  report(err, errsz, "%s: bad value%s\n", e->key, "");
  return 0;
}

static const Entry *find(const char *key) {
  for (int i = 0; i < N_ENTRIES; i++)
    if (!strcmp(k_entries[i].key, key)) return &k_entries[i];
  return NULL;
}

// Walks tables, building dotted paths, so [glass] fill = ..., "glass.fill" = ...
// and glass.fill = ... all name the same token.
static int walk(UiTokens *tok, const toml_table_t *tab, const char *prefix, char *err, int errsz) {
  int applied = 0;
  for (int i = 0;; i++) {
    const char *k = toml_key_in(tab, i);
    if (!k) break;
    char path[160];
    snprintf(path, sizeof path, "%s%s%s", prefix, *prefix ? "." : "", k);
    const Entry *e = find(path);
    if (e) {
      applied += apply(tok, e, tab, k, err, errsz);
      continue;
    }
    toml_table_t *sub = toml_table_in(tab, k);
    if (sub) {
      applied += walk(tok, sub, path, err, errsz);
      continue;
    }
    if (!strcmp(path, "format") || !strcmp(path, "name") || !strcmp(path, "description")) continue;
    report(err, errsz, "%s: unknown token%s\n", path, "");
  }
  return applied;
}

int ui_tokens_load_string(UiTokens *t, const char *toml, char *err, int errsz) {
  char ebuf[200];
  char *copy = strdup(toml);
  if (!copy) return -1;
  toml_table_t *root = toml_parse(copy, ebuf, sizeof ebuf);
  if (!root) {
    report(err, errsz, "toml: %s%s\n", ebuf, "");
    free(copy);
    return -1;
  }
  int n = walk(t, root, "", err, errsz);
  toml_free(root);
  free(copy);
  return n;
}

int ui_tokens_load(UiTokens *t, const char *path, char *err, int errsz) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    report(err, errsz, "%s: cannot open%s\n", path, "");
    return -1;
  }
  char ebuf[200];
  toml_table_t *root = toml_parse_file(f, ebuf, sizeof ebuf);
  fclose(f);
  if (!root) {
    report(err, errsz, "%s: %s\n", path, ebuf);
    return -1;
  }
  int n = walk(t, root, "", err, errsz);
  toml_free(root);
  return n;
}

int ui_tokens_set(UiTokens *t, const char *key, const char *toml_value, char *err, int errsz) {
  char buf[512];
  snprintf(buf, sizeof buf, "\"%s\" = %s\n", key, toml_value);
  return ui_tokens_load_string(t, buf, err, errsz);
}
