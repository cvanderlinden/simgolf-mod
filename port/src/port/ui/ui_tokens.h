// Frosted Mint UI kit (simgolf mod, task-26): design tokens.
//
// Every colour, radius, spacing, shadow and type style the kit draws with
// comes from here, loaded from data/ui/frosted_mint.toml (docs/ui/KIT.md).
// The defaults below are the same values (docs/ui/DESIGN_SYSTEM.md section 2),
// so the kit still draws if the file is missing. Mods restyle by shipping
// ui/frosted_mint.toml with only the keys they change (ui_tokens_load_mods).
//
// Sizes are UI pixels at 1080p (UI scale 1.0); the kit multiplies them by the
// UI scale (ui_layout.h) when it draws.
#pragma once

#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } UiColor;
typedef struct { UiColor from, to; float angle; } UiGrad;              // CSS linear-gradient angle, degrees
typedef struct { float x, y, blur; UiColor color; } UiShadow;           // CSS box-shadow, no spread
typedef struct { float size, line, tracking; int weight; } UiType;      // px, px, em, 400..800

enum {
  UI_TYPE_DISPLAY, UI_TYPE_TITLE, UI_TYPE_HEADING, UI_TYPE_NUMBER, UI_TYPE_BODY,
  UI_TYPE_LABEL, UI_TYPE_CAPTION, UI_TYPE_MICRO, UI_TYPE_QUOTE, UI_TYPE_COUNT
};

typedef struct UiTokens {
  // colour
  UiColor glass_fill, glass_fill_strong, glass_border, glass_highlight, glass_solid;
  UiShadow glass_shadow;
  float glass_blur;            // backdrop blur radius (CSS px); 0 = no blur, glass.solid instead
  float glass_saturation;      // kept for mods; the SDL presenter has no saturate step (KIT.md)
  UiColor surface_inset, surface_inset_border;
  UiColor text_primary, text_secondary, text_tertiary, text_on_accent;
  UiColor accent, accent_soft, accent_soft_text, money, danger;
  UiGrad accent_grad, warn_grad, meter_grad;
  UiColor meter_track, divider, seg_track, seg_on, tile_fill, tile_fill_on, dock_fill, icon_muted;
  UiShadow seg_on_shadow, dock_square_shadow, dock_active_shadow, thumb_shadow;
  UiColor world_bg_top, world_bg_mid, world_bg_bottom;
  UiColor scrim;
  // radius
  float radius_panel, radius_card, radius_tile, radius_menu_tile, radius_control, radius_seg_inner, radius_tray;
  // spacing
  float space[8];
  float pad_panel, pad_tray, pad_card, pad_dialog, gap_tiles, edge_margin;
  // components (sizes at scale 1)
  float capsule_h, capsule_pad, emblem, dock_item_w, dock_square, dock_square_on, dock_icon, dock_pad_x, dock_pad_top,
      dock_pad_bottom, dock_bottom, tray_bottom, tile_w, tile_art_w, tile_art_h, icon_button, icon_button_sm, button_h,
      pill_h, seg_h, tooltip_h, tooltip_delay_ms, icon_stroke, meter_h, slider_track_h, slider_thumb, toggle_w, toggle_h,
      dialog_min_w, dialog_max_w, popover_w, popover_arrow, bubble_max_w, cam_button;
  // type
  UiType type[UI_TYPE_COUNT];
  float text_floor;            // smallest text in screen px (design: 11)
  float text_gamma;            // coverage curve for glyphs (1 = linear coverage)
  // motion (ms)
  float motion_hover, motion_press, motion_open, motion_close;
} UiTokens;

// The built-in Frosted Mint values.
void ui_tokens_default(UiTokens *t);

// Applies a TOML file on top of t. Unknown keys and bad values are reported in
// err (one line each, up to errsz bytes) and skipped. Returns the number of
// keys applied, -1 when the file cannot be read or parsed.
int ui_tokens_load(UiTokens *t, const char *path, char *err, int errsz);

// The same from a string (tests).
int ui_tokens_load_string(UiTokens *t, const char *toml, char *err, int errsz);

// One key by its dotted name ("glass.fill", "radius.panel", "type.title"):
// value is TOML text ("\"#0E9F6E\"", "30", "{ size = 24, line = 30, weight = 800 }").
int ui_tokens_set(UiTokens *t, const char *key, const char *toml_value, char *err, int errsz);

// "#RRGGBB", "#RRGGBBAA", "rgb(r,g,b)", "rgba(r,g,b,a)" (a 0..1). 1 on success.
int ui_color_parse(const char *s, UiColor *out);

// Key names, for docs and the token check (NULL past the end).
const char *ui_tokens_key(int i);
